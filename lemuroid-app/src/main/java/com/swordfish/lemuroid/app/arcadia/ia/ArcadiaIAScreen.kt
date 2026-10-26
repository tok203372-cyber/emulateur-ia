package com.swordfish.lemuroid.app.arcadia.ia

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
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
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Chat
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.RecordVoiceOver
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.SwapHoriz
import androidx.compose.material.icons.filled.Translate
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.viewModelScope
import com.google.mlkit.common.model.DownloadConditions
import com.google.mlkit.common.model.RemoteModelManager
import com.google.mlkit.nl.translate.TranslateLanguage
import com.google.mlkit.nl.translate.TranslateRemoteModel
import com.swordfish.lemuroid.app.arcadia.ui.ArcadiaCouleurs
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import timber.log.Timber

class ArcadiaIAViewModel(private val appContext: Context) : ViewModel() {
    class Factory(private val appContext: Context) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T = ArcadiaIAViewModel(appContext) as T
    }

    data class Etat(
        val compagnonInstalle: Boolean = false,
        val langueCible: String = TranslateLanguage.FRENCH,
        val languesPretes: Set<String> = emptySet(),
        val telechargements: Set<String> = emptySet(),
        val lectureAuto: Boolean = false,
        val intervalle: Long = 900L,
        val message: String? = null,
    )

    private val etatInterne = MutableStateFlow(Etat())
    val etat: StateFlow<Etat> = etatInterne

    private val gestionnaire by lazy {
        LecteurEcran.initialiserMlKit(appContext)
        RemoteModelManager.getInstance()
    }

    init {
        rafraichir()
    }

    fun rafraichir() {
        etatInterne.value =
            etatInterne.value.copy(
                compagnonInstalle = CompagnonIA.estInstalle(appContext),
                langueCible = ReglagesIA.langueCible(appContext),
                lectureAuto = ReglagesIA.lectureAuto(appContext),
                intervalle = ReglagesIA.intervalle(appContext),
            )
        viewModelScope.launch {
            val pretes =
                runCatching {
                    gestionnaire.getDownloadedModels(TranslateRemoteModel::class.java).attendre()
                        .map { it.language }
                        .toSet()
                }.onFailure { Timber.w(it, "Arcadia : modèles de traduction illisibles") }
                    .getOrDefault(emptySet())
            etatInterne.value = etatInterne.value.copy(languesPretes = pretes)
        }
    }

    /** Télécharge la langue du jeu et la langue d'arrivée (environ 30 Mo chacune, une seule fois). */
    fun preparer(langueSource: String) {
        val cible = etatInterne.value.langueCible
        val aTelecharger = listOf(langueSource, cible).filter { it != TranslateLanguage.ENGLISH }.distinct()
        etatInterne.value = etatInterne.value.copy(telechargements = etatInterne.value.telechargements + langueSource)
        viewModelScope.launch {
            val reussi =
                runCatching {
                    aTelecharger.forEach { langue ->
                        gestionnaire.download(
                            TranslateRemoteModel.Builder(langue).build(),
                            DownloadConditions.Builder().build(),
                        ).attendre()
                    }
                }.onFailure { Timber.w(it, "Arcadia : téléchargement de la langue impossible") }
                    .isSuccess
            etatInterne.value =
                etatInterne.value.copy(
                    telechargements = etatInterne.value.telechargements - langueSource,
                    message =
                        if (reussi) {
                            "${LecteurEcran.nomLangue(langueSource).replaceFirstChar { it.uppercase() }} prêt : traduction hors ligne disponible."
                        } else {
                            "Téléchargement impossible. Vérifiez la connexion Internet et réessayez."
                        },
                )
            rafraichir()
        }
    }

    fun definirLangueCible(code: String) {
        ReglagesIA.definirLangueCible(appContext, code)
        rafraichir()
    }

    fun definirLectureAuto(actif: Boolean) {
        ReglagesIA.definirLectureAuto(appContext, actif)
        rafraichir()
    }

    fun definirIntervalle(ms: Long) {
        ReglagesIA.definirIntervalle(appContext, ms)
        rafraichir()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ArcadiaIAScreen(
    modifier: Modifier = Modifier,
    viewModel: ArcadiaIAViewModel,
    onDiscuter: () -> Unit,
    onQueJouer: () -> Unit,
    onReglagesManettes: () -> Unit,
) {
    val etat by viewModel.etat.collectAsState()

    val cycleDeVie = LocalLifecycleOwner.current
    DisposableEffect(cycleDeVie) {
        val observateur =
            LifecycleEventObserver { _, evenement ->
                if (evenement == Lifecycle.Event.ON_RESUME) viewModel.rafraichir()
            }
        cycleDeVie.lifecycle.addObserver(observateur)
        onDispose { cycleDeVie.lifecycle.removeObserver(observateur) }
    }

    LazyColumn(
        modifier = modifier.fillMaxSize(),
        contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 12.dp, bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        item {
            Box(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(22.dp))
                        .background(ArcadiaCouleurs.degradeIA)
                        .padding(20.dp),
            ) {
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(Icons.Filled.AutoAwesome, contentDescription = null, tint = Color.White)
                        Text(
                            text = "  Arcadia IA",
                            style = MaterialTheme.typography.headlineSmall,
                            fontWeight = FontWeight.Bold,
                            color = Color.White,
                        )
                    }
                    Text(
                        text =
                            "Une intelligence artificielle qui fonctionne entièrement sur votre téléphone : " +
                                "rien n'est envoyé sur Internet, et elle marche même en avion.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = Color.White.copy(alpha = 0.92f),
                    )
                }
            }
        }

        item {
            Carte(titre = "Assistant de jeu", icone = Icons.Filled.Chat) {
                if (etat.compagnonInstalle) {
                    Text(
                        "Posez vos questions sur un jeu, demandez une astuce ou un conseil. Pendant une partie, " +
                            "l'assistant voit votre écran.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = onDiscuter) { Text("Discuter avec l'IA") }
                        FilledTonalButton(onClick = onQueJouer) { Text("Que jouer ce soir ?") }
                    }
                } else {
                    Text(
                        "Pour l'assistant, installez aussi l'application « Arcadia IA » (le fichier " +
                            "Arcadia-IA.apk fourni avec Arcadia). La traduction et la commande vocale " +
                            "marchent déjà sans elle.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            }
        }

        item {
            Carte(titre = "Traduction hors ligne", icone = Icons.Filled.Translate) {
                Text(
                    "Traduit les textes des jeux (japonais, anglais…) directement sur l'image. " +
                        "Préparez une langue une fois avec Internet, ensuite plus besoin de connexion.",
                    style = MaterialTheme.typography.bodyMedium,
                )
                Text("Traduire vers :", style = MaterialTheme.typography.labelLarge)
                LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(ReglagesIA.languesCibles, key = { it.code }) { langue ->
                        FilterChip(
                            selected = etat.langueCible == langue.code,
                            onClick = { viewModel.definirLangueCible(langue.code) },
                            label = { Text(langue.nom) },
                        )
                    }
                }
                Text("Langues des jeux :", style = MaterialTheme.typography.labelLarge)
                ReglagesIA.languesSources.filter { it.code != etat.langueCible }.forEach { langue ->
                    val prete =
                        (langue.code == TranslateLanguage.ENGLISH || langue.code in etat.languesPretes) &&
                            (etat.langueCible == TranslateLanguage.ENGLISH || etat.langueCible in etat.languesPretes)
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            text = langue.nom,
                            style = MaterialTheme.typography.bodyLarge,
                            modifier = Modifier.weight(1f),
                        )
                        when {
                            langue.code in etat.telechargements ->
                                CircularProgressIndicator(modifier = Modifier.size(22.dp), strokeWidth = 2.dp)
                            prete ->
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Icon(
                                        Icons.Filled.CheckCircle,
                                        contentDescription = null,
                                        tint = ArcadiaCouleurs.cyan,
                                        modifier = Modifier.size(18.dp),
                                    )
                                    Text(" Prête", style = MaterialTheme.typography.labelLarge)
                                }
                            else ->
                                OutlinedButton(onClick = { viewModel.preparer(langue.code) }) {
                                    Icon(Icons.Filled.Download, contentDescription = null, modifier = Modifier.size(16.dp))
                                    Spacer(Modifier.width(6.dp))
                                    Text("Préparer (~30 Mo)")
                                }
                        }
                    }
                }
                etat.message?.let {
                    Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.tertiary)
                }
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text("Lire les traductions à voix haute", style = MaterialTheme.typography.bodyLarge)
                        Text(
                            "Pendant la traduction en direct",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Switch(checked = etat.lectureAuto, onCheckedChange = { viewModel.definirLectureAuto(it) })
                }
                Text("Vitesse de la traduction en direct :", style = MaterialTheme.typography.labelLarge)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(600L to "Rapide", 900L to "Normale", 1500L to "Économe").forEach { (ms, nom) ->
                        FilterChip(
                            selected = etat.intervalle == ms,
                            onClick = { viewModel.definirIntervalle(ms) },
                            label = { Text(nom) },
                        )
                    }
                }
            }
        }

        item {
            Carte(titre = "Pendant vos parties", icone = Icons.Filled.SportsEsports) {
                Text(
                    "Ouvrez le menu du jeu (bouton menu à l'écran, ou Select + Start sur la manette) :",
                    style = MaterialTheme.typography.bodyMedium,
                )
                Fonction(Icons.Filled.SwapHoriz, "Changer de jeu ou de console", "La partie est sauvegardée, l'autre jeu démarre avec le bon émulateur.")
                Fonction(Icons.Filled.Translate, "Traduction en direct", "Les textes du jeu sont remplacés par leur traduction.")
                Fonction(Icons.Filled.RecordVoiceOver, "Lire l'écran à voix haute", "L'IA lit (et traduit) ce qui est écrit.")
                Fonction(Icons.Filled.Mic, "Commande vocale", "« Sauvegarde », « traduis », « change de jeu pour Zelda »…")
                Fonction(Icons.Filled.Visibility, "Assistant qui voit l'écran", "Bloqué ? Demandez de l'aide : l'IA lit votre écran.")
            }
        }

        item {
            Carte(titre = "Manettes", icone = Icons.Filled.Gamepad) {
                Text(
                    "Branchez ou appairez votre manette : Arcadia règle les touches tout seul pour chaque " +
                        "console. Vous pouvez les changer si vous le souhaitez.",
                    style = MaterialTheme.typography.bodyMedium,
                )
                OutlinedButton(onClick = onReglagesManettes) { Text("Réglages des manettes") }
            }
        }
    }
}

@Composable
private fun Carte(
    titre: String,
    icone: ImageVector,
    contenu: @Composable () -> Unit,
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.55f)),
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(icone, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                Text(
                    text = "  $titre",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                )
            }
            contenu()
        }
    }
}

@Composable
private fun Fonction(
    icone: ImageVector,
    titre: String,
    detail: String,
) {
    Row(verticalAlignment = Alignment.Top) {
        Icon(
            icone,
            contentDescription = null,
            tint = ArcadiaCouleurs.cyan,
            modifier =
                Modifier
                    .padding(top = 2.dp)
                    .size(20.dp),
        )
        Column(modifier = Modifier.padding(start = 10.dp)) {
            Text(titre, style = MaterialTheme.typography.bodyLarge, fontWeight = FontWeight.Medium)
            Text(detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
