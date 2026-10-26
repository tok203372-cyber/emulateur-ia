package app.arcadia.ia

import android.app.Application
import android.app.DownloadManager
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.StatFs
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Chat
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.FileOpen
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.OpenInBrowser
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File
import java.util.Locale

class AccueilViewModel(application: Application) : AndroidViewModel(application) {
    data class LigneModele(
        val modele: ModeleIA,
        val installe: Boolean,
        val choisi: Boolean,
        val progression: Telechargements.Progression?,
    )

    data class Etat(
        val lignes: List<LigneModele> = emptyList(),
        val importes: List<Pair<ModeleIA, File>> = emptyList(),
        val gpu: Boolean = true,
        val lectureAuto: Boolean = false,
        val importEnCours: Float? = null,
        val message: String? = null,
    )

    private val app = application
    private val etatInterne = MutableStateFlow(Etat())
    val etat: StateFlow<Etat> = etatInterne
    val moteur: StateFlow<MoteurIA.Etat> = MoteurIA.etat

    init {
        rafraichir()
        viewModelScope.launch {
            while (isActive) {
                delay(1000)
                if (Modeles.telechargementEnCours(app).isNotEmpty()) rafraichir()
            }
        }
    }

    fun rafraichir() {
        val choisi = Modeles.modeleChoisi(app)
        val enCours = Modeles.telechargementEnCours(app)
        var message = etatInterne.value.message
        val lignes =
            Modeles.catalogue.map { m ->
                var progression = enCours[m.id]?.let { Telechargements.progression(app, it) }
                if (progression != null && !progression.enCours) {
                    // Téléchargement terminé (réussi ou non) : on arrête de le suivre.
                    Modeles.noterTelechargement(app, m.id, null)
                    if (progression.statut == DownloadManager.STATUS_FAILED) {
                        message = messageEchec(m, progression.raison)
                        Modeles.fichier(app, m).delete()
                    } else if (progression.statut == DownloadManager.STATUS_SUCCESSFUL) {
                        message = "${m.nom} est installé ! Vous pouvez l'utiliser."
                        if (choisi == null) Modeles.choisir(app, m)
                    }
                    progression = null
                }
                LigneModele(
                    modele = m,
                    installe = Modeles.estInstalle(app, m),
                    choisi = choisi?.fichier == m.fichier,
                    progression = progression,
                )
            }
        val importes = Modeles.importes(app).map { Modeles.depuisFichier(it) to it }
        etatInterne.update {
            it.copy(
                lignes = lignes,
                importes = importes,
                gpu = Modeles.utiliserGpu(app),
                lectureAuto = Modeles.lectureAuto(app),
                message = message,
            )
        }
    }

    private fun messageEchec(
        modele: ModeleIA,
        raison: Int,
    ): String =
        when (raison) {
            401, 403 ->
                "${modele.nom} demande un compte Hugging Face. Ouvrez sa page, acceptez la licence, " +
                    "téléchargez le fichier puis utilisez « Importer un fichier »."
            DownloadManager.ERROR_INSUFFICIENT_SPACE -> "Pas assez de place sur le téléphone pour ${modele.nom}."
            else -> "Le téléchargement de ${modele.nom} a échoué (code $raison). Vérifiez Internet et réessayez."
        }

    fun telecharger(modele: ModeleIA) {
        val libre = runCatching { StatFs(Modeles.dossier(app).absolutePath).availableBytes }.getOrDefault(Long.MAX_VALUE)
        if (libre < modele.taille + 300L * 1024 * 1024) {
            etatInterne.update { it.copy(message = "Il faut au moins ${taille(modele.taille + 300L * 1024 * 1024)} de libre.") }
            return
        }
        Telechargements.demarrer(app, modele)
        etatInterne.update { it.copy(message = "Téléchargement de ${modele.nom} lancé (suivez-le aussi dans les notifications).") }
        rafraichir()
    }

    fun annuler(modele: ModeleIA) {
        Telechargements.annuler(app, modele)
        rafraichir()
    }

    fun choisir(modele: ModeleIA) {
        Modeles.choisir(app, modele)
        viewModelScope.launch { MoteurIA.fermer() }
        rafraichir()
    }

    fun supprimer(fichier: File) {
        viewModelScope.launch {
            MoteurIA.fermer()
            Telechargements.supprimer(app, fichier)
            rafraichir()
        }
    }

    fun importer(uri: Uri) {
        etatInterne.update { it.copy(importEnCours = 0f, message = null) }
        viewModelScope.launch {
            val resultat = Telechargements.importer(app, uri) { p -> etatInterne.update { it.copy(importEnCours = p) } }
            etatInterne.update {
                it.copy(
                    importEnCours = null,
                    message =
                        resultat.fold(
                            onSuccess = { f -> "Modèle importé : ${f.name}" },
                            onFailure = { e -> "Import impossible : ${e.message}" },
                        ),
                )
            }
            resultat.getOrNull()?.let { f -> Modeles.choisir(app, Modeles.depuisFichier(f)) }
            MoteurIA.fermer()
            rafraichir()
        }
    }

    fun definirGpu(actif: Boolean) {
        Modeles.definirGpu(app, actif)
        viewModelScope.launch { MoteurIA.fermer() }
        rafraichir()
    }

    fun definirLectureAuto(actif: Boolean) {
        Modeles.definirLectureAuto(app, actif)
        rafraichir()
    }

    fun liberer() {
        viewModelScope.launch { MoteurIA.fermer() }
    }

    fun precharger() {
        viewModelScope.launch { MoteurIA.preparer(app) }
    }
}

fun taille(octets: Long): String {
    val go = octets / (1024.0 * 1024 * 1024)
    return if (go >= 1) String.format(Locale.FRANCE, "%.1f Go", go) else String.format(Locale.FRANCE, "%.0f Mo", octets / (1024.0 * 1024))
}

class AccueilActivity : ComponentActivity() {
    private val viewModel: AccueilViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContent {
            ThemeArcadia {
                EcranAccueil(
                    viewModel = viewModel,
                    onDiscuter = { startActivity(Intent(this, AssistantActivity::class.java)) },
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EcranAccueil(
    viewModel: AccueilViewModel,
    onDiscuter: () -> Unit,
) {
    val etat by viewModel.etat.collectAsState()
    val moteur by viewModel.moteur.collectAsState()
    val context = LocalContext.current

    val choixFichier =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri != null) viewModel.importer(uri)
        }

    val cycleDeVie = LocalLifecycleOwner.current
    DisposableEffect(cycleDeVie) {
        val observateur =
            LifecycleEventObserver { _, e -> if (e == Lifecycle.Event.ON_RESUME) viewModel.rafraichir() }
        cycleDeVie.lifecycle.addObserver(observateur)
        onDispose { cycleDeVie.lifecycle.removeObserver(observateur) }
    }

    fun ouvrir(lien: String) {
        runCatching { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(lien))) }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Arcadia IA") },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
            )
        },
    ) { marges ->
        LazyColumn(
            modifier =
                Modifier
                    .fillMaxSize()
                    .padding(marges),
            contentPadding = PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            item {
                Banniere(
                    titre = "Arcadia IA",
                    texte =
                        "Le cerveau d'Arcadia : un modèle d'intelligence artificielle qui tourne entièrement sur " +
                            "votre téléphone. Vos questions ne quittent jamais l'appareil.",
                    icone = Icons.Filled.AutoAwesome,
                )
            }

            item {
                CarteArcadia(titre = "Modèle actif", icone = Icons.Filled.Memory) {
                    val choisi = etat.lignes.firstOrNull { it.choisi }?.modele ?: etat.importes.firstOrNull()?.first
                    Text(
                        text = choisi?.nom ?: "Aucun modèle installé pour l'instant : choisissez-en un ci-dessous.",
                        style = MaterialTheme.typography.bodyLarge,
                        fontWeight = FontWeight.Medium,
                    )
                    Text(
                        text = "État : " + descriptionMoteur(moteur),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    if (moteur is MoteurIA.Etat.Chargement) LinearProgressIndicator(Modifier.fillMaxWidth())
                    (moteur as? MoteurIA.Etat.Erreur)?.let { Text(it.message, color = MaterialTheme.colorScheme.error) }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = onDiscuter, enabled = choisi != null) {
                            Icon(Icons.Filled.Chat, contentDescription = null, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(6.dp))
                            Text("Discuter")
                        }
                        if (moteur is MoteurIA.Etat.Pret) {
                            OutlinedButton(onClick = { viewModel.liberer() }) { Text("Libérer la mémoire") }
                        } else if (choisi != null) {
                            OutlinedButton(onClick = { viewModel.precharger() }) { Text("Préparer") }
                        }
                    }
                }
            }

            etat.message?.let { m ->
                item { Text(m, color = MaterialTheme.colorScheme.tertiary, style = MaterialTheme.typography.bodyMedium) }
            }

            item {
                Text(
                    "Modèles disponibles",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.padding(top = 4.dp),
                )
            }

            items(etat.lignes, key = { it.modele.id }) { ligne ->
                val m = ligne.modele
                CarteArcadia {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        if (ligne.installe) {
                            RadioButton(selected = ligne.choisi, onClick = { viewModel.choisir(m) })
                        }
                        Column(Modifier.weight(1f)) {
                            Text(m.nom, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                            Text(taille(m.taille), style = MaterialTheme.typography.labelMedium, color = Couleurs.cyan)
                        }
                        if (ligne.installe) {
                            Icon(Icons.Filled.CheckCircle, contentDescription = "Installé", tint = Couleurs.cyan)
                        }
                    }
                    Text(m.description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    val p = ligne.progression
                    when {
                        p != null -> {
                            LinearProgressIndicator(progress = { p.fraction }, modifier = Modifier.fillMaxWidth())
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(
                                    "${(p.fraction * 100).toInt()} % — ${taille(p.octets)} / ${taille(if (p.total > 0) p.total else m.taille)}",
                                    style = MaterialTheme.typography.labelMedium,
                                    modifier = Modifier.weight(1f),
                                )
                                TextButton(onClick = { viewModel.annuler(m) }) { Text("Annuler") }
                            }
                        }
                        ligne.installe ->
                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                if (!ligne.choisi) {
                                    FilledTonalButton(onClick = { viewModel.choisir(m) }) { Text("Utiliser") }
                                }
                                TextButton(onClick = { viewModel.supprimer(Modeles.fichier(context, m)) }) {
                                    Icon(Icons.Filled.Delete, contentDescription = null, modifier = Modifier.size(18.dp))
                                    Text(" Supprimer")
                                }
                            }
                        m.url != null ->
                            Button(onClick = { viewModel.telecharger(m) }) {
                                Icon(Icons.Filled.Download, contentDescription = null, modifier = Modifier.size(18.dp))
                                Spacer(Modifier.width(6.dp))
                                Text("Télécharger (${taille(m.taille)})")
                            }
                        else ->
                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                m.page?.let { lien ->
                                    OutlinedButton(onClick = { ouvrir(lien) }) {
                                        Icon(Icons.Filled.OpenInBrowser, contentDescription = null, modifier = Modifier.size(18.dp))
                                        Text(" Ouvrir la page")
                                    }
                                }
                                FilledTonalButton(onClick = { choixFichier.launch(arrayOf("*/*")) }) { Text("Importer") }
                            }
                    }
                }
            }

            if (etat.importes.isNotEmpty()) {
                item {
                    Text("Modèles importés", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                }
                items(etat.importes, key = { it.second.name }) { (m, fichier) ->
                    val choisi = Modeles.modeleChoisi(context)?.fichier == fichier.name
                    CarteArcadia {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            RadioButton(selected = choisi, onClick = { viewModel.choisir(m) })
                            Column(Modifier.weight(1f)) {
                                Text(m.nom, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                                Text(
                                    taille(fichier.length()) + if (m.vision) " • comprend les images" else "",
                                    style = MaterialTheme.typography.labelMedium,
                                    color = Couleurs.cyan,
                                )
                            }
                            TextButton(onClick = { viewModel.supprimer(fichier) }) { Text("Supprimer") }
                        }
                    }
                }
            }

            item {
                CarteArcadia(titre = "Importer un modèle", icone = Icons.Filled.FileOpen) {
                    Text(
                        "Vous avez déjà un fichier .litertlm (par exemple Gemma 3n téléchargé sur Hugging Face) ? " +
                            "Importez-le ici.",
                        style = MaterialTheme.typography.bodySmall,
                    )
                    val p = etat.importEnCours
                    if (p != null) {
                        LinearProgressIndicator(progress = { p }, modifier = Modifier.fillMaxWidth())
                        Text("Copie en cours… ${(p * 100).toInt()} %", style = MaterialTheme.typography.labelMedium)
                    } else {
                        FilledTonalButton(onClick = { choixFichier.launch(arrayOf("*/*")) }) { Text("Importer un fichier") }
                    }
                }
            }

            item {
                CarteArcadia(titre = "Réglages", icone = Icons.Filled.Settings) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text("Utiliser la puce graphique", style = MaterialTheme.typography.bodyLarge)
                            Text(
                                "Beaucoup plus rapide. Désactivez seulement en cas de problème.",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        Switch(checked = etat.gpu, onCheckedChange = { viewModel.definirGpu(it) })
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text("Lire les réponses à voix haute", style = MaterialTheme.typography.bodyLarge)
                            Text(
                                "Automatiquement, avec la voix du téléphone.",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        Switch(checked = etat.lectureAuto, onCheckedChange = { viewModel.definirLectureAuto(it) })
                    }
                }
            }

            item {
                CarteArcadia(titre = "Bon à savoir", icone = Icons.Filled.Info) {
                    Text(
                        "• Le modèle se télécharge une seule fois ; ensuite tout marche sans Internet.\n" +
                            "• Une petite IA peut se tromper sur des détails précis d'un jeu : vérifiez si c'est important.\n" +
                            "• Modèles : Qwen 2.5 (Alibaba, Apache 2.0), Phi-4 mini (Microsoft, MIT), " +
                            "Gemma 3n (Google, conditions d'utilisation de Gemma).",
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
        }
    }
}
