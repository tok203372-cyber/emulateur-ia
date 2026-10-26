package app.arcadia.ia

import android.app.Application
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.BitmapFactory
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Send
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material.icons.filled.VolumeUp
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledIconButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SuggestionChip
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.google.ai.edge.litertlm.Conversation
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Les informations envoyées par Arcadia quand on demande de l'aide pendant une partie. */
data class ContexteJeu(
    val titre: String?,
    val console: String?,
    val texteEcran: String?,
    val image: ByteArray?,
)

class AssistantViewModel(application: Application) : AndroidViewModel(application) {
    data class MessageChat(val id: Long, val deMoi: Boolean, val texte: String, val enCours: Boolean = false)

    data class Etat(
        val messages: List<MessageChat> = emptyList(),
        val reponseEnCours: Boolean = false,
        val contexte: ContexteJeu? = null,
        val pret: Boolean = false,
    )

    private val etatInterne = MutableStateFlow(Etat())
    val etat: StateFlow<Etat> = etatInterne
    val moteur: StateFlow<MoteurIA.Etat> = MoteurIA.etat

    private val app = application
    private var conversation: Conversation? = null
    private var imageEnAttente: ByteArray? = null
    private var demarre = false
    private var tache: Job? = null
    private var compteur = 0L
    val lecteur = LecteurVocal(application)

    fun demarrer(
        contexte: ContexteJeu?,
        question: String?,
    ) {
        if (demarre) return
        demarre = true
        etatInterne.value = Etat(contexte = contexte)
        viewModelScope.launch {
            val pret = MoteurIA.preparer(app) ?: return@launch
            val conv = MoteurIA.nouvelleConversation(app, Consignes.assistant(contexte, pret.vision)) ?: return@launch
            conversation = conv
            imageEnAttente = contexte?.image?.takeIf { pret.vision }
            etatInterne.update { it.copy(pret = true) }
            if (!question.isNullOrBlank()) {
                envoyer(question)
            } else if (contexte?.titre != null) {
                ajouter(
                    MessageChat(
                        ++compteur,
                        false,
                        "Je vois que vous jouez à **${contexte.titre}**. " +
                            (if (contexte.texteEcran != null || imageEnAttente != null) "J'ai regardé votre écran. " else "") +
                            "Posez-moi votre question !",
                    ),
                )
            }
        }
    }

    private fun ajouter(message: MessageChat) {
        etatInterne.update { it.copy(messages = it.messages + message) }
    }

    private fun majDerniere(
        texte: String,
        enCours: Boolean,
    ) {
        etatInterne.update { e ->
            val liste = e.messages.toMutableList()
            val i = liste.indexOfLast { !it.deMoi }
            if (i >= 0) liste[i] = liste[i].copy(texte = texte, enCours = enCours)
            e.copy(messages = liste)
        }
    }

    fun envoyer(texte: String) {
        val conv = conversation ?: return
        val propre = texte.trim()
        if (propre.isEmpty() || etatInterne.value.reponseEnCours) return
        ajouter(MessageChat(++compteur, true, propre))
        ajouter(MessageChat(++compteur, false, "", enCours = true))
        etatInterne.update { it.copy(reponseEnCours = true) }
        val image = imageEnAttente
        imageEnAttente = null
        tache =
            viewModelScope.launch {
                val reponse = StringBuilder()
                try {
                    MoteurIA.repondre(conv, propre, image).collect { morceau ->
                        reponse.append(morceau)
                        majDerniere(reponse.toString(), true)
                    }
                    majDerniere(reponse.toString().trim(), false)
                    if (Modeles.lectureAuto(app)) lecteur.lire(reponse.toString())
                } catch (e: CancellationException) {
                    majDerniere(reponse.toString().trim(), false)
                    throw e
                } catch (e: Exception) {
                    majDerniere(reponse.toString().trim().ifEmpty { "Désolé, une erreur est survenue : ${e.message}" }, false)
                } finally {
                    etatInterne.update { it.copy(reponseEnCours = false) }
                }
            }
    }

    fun arreter() {
        MoteurIA.interrompre()
    }

    fun recommencer() {
        MoteurIA.interrompre()
        tache?.cancel()
        val contexte = etatInterne.value.contexte
        demarre = false
        conversation = null
        demarrer(contexte?.copy(image = null), null)
    }

    override fun onCleared() {
        MoteurIA.interrompre()
        lecteur.fermer()
        super.onCleared()
    }
}

class AssistantActivity : ComponentActivity() {
    private val viewModel: AssistantViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        val titre = intent.getStringExtra(Contrat.EXTRA_TITRE_JEU)
        val console = intent.getStringExtra(Contrat.EXTRA_CONSOLE)
        val texteEcran = intent.getStringExtra(Contrat.EXTRA_TEXTE_ECRAN)
        val question = intent.getStringExtra(Contrat.EXTRA_QUESTION)
        val uriImage = intent.data

        setContent {
            ThemeArcadia {
                var imageChargee by remember { mutableStateOf(uriImage == null) }
                var octetsImage by remember { mutableStateOf<ByteArray?>(null) }
                LaunchedEffect(uriImage) {
                    if (uriImage != null) {
                        octetsImage =
                            withContext(Dispatchers.IO) {
                                runCatching { contentResolver.openInputStream(uriImage)?.use { it.readBytes() } }.getOrNull()
                            }
                        imageChargee = true
                    }
                }
                LaunchedEffect(imageChargee) {
                    if (imageChargee) {
                        val contexte =
                            if (titre != null || texteEcran != null || octetsImage != null) {
                                ContexteJeu(titre, console, texteEcran, octetsImage)
                            } else {
                                null
                            }
                        viewModel.demarrer(contexte, question)
                    }
                }
                EcranAssistant(viewModel = viewModel, onFermer = { finish() })
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EcranAssistant(
    viewModel: AssistantViewModel,
    onFermer: () -> Unit,
) {
    val etat by viewModel.etat.collectAsState()
    val moteur by viewModel.moteur.collectAsState()
    val context = LocalContext.current
    var saisie by rememberSaveable { mutableStateOf("") }
    val liste = rememberLazyListState()

    val dictee =
        rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { r ->
            resultatDictee(r.data)?.let { phrase ->
                if (etat.pret) viewModel.envoyer(phrase) else saisie = phrase
            }
        }

    LaunchedEffect(etat.messages.size, etat.messages.lastOrNull()?.texte?.length) {
        if (etat.messages.isNotEmpty()) liste.animateScrollToItem(etat.messages.lastIndex)
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text("Assistant Arcadia", style = MaterialTheme.typography.titleLarge)
                        Text(
                            text = descriptionMoteur(moteur),
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                },
                navigationIcon = {
                    IconButton(onClick = onFermer) { Icon(Icons.Filled.Close, contentDescription = "Fermer") }
                },
                actions = {
                    IconButton(onClick = { viewModel.recommencer() }) {
                        Icon(Icons.Filled.Refresh, contentDescription = "Nouvelle conversation")
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
            )
        },
    ) { marges ->
        Column(
            modifier =
                Modifier
                    .fillMaxSize()
                    .padding(marges)
                    .imePadding(),
        ) {
            when (val m = moteur) {
                is MoteurIA.Etat.Chargement ->
                    Column(Modifier.padding(16.dp)) {
                        Text("Chargement de ${m.nom}… (quelques secondes la première fois)")
                        LinearProgressIndicator(Modifier.fillMaxWidth().padding(top = 8.dp))
                    }
                is MoteurIA.Etat.AucunModele ->
                    Box(Modifier.padding(16.dp)) {
                        CarteArcadia(titre = "Aucun modèle d'IA installé", icone = Icons.Filled.AutoAwesome) {
                            Text("Téléchargez d'abord un modèle (une seule fois, environ 2 à 4 Go).")
                            Button(onClick = {
                                context.startActivity(Intent(context, AccueilActivity::class.java))
                                onFermer()
                            }) { Text("Choisir un modèle") }
                        }
                    }
                is MoteurIA.Etat.Erreur ->
                    Text(m.message, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(16.dp))
                else -> Unit
            }

            etat.contexte?.let { ctx -> BandeauContexte(ctx) }

            LazyColumn(
                modifier =
                    Modifier
                        .weight(1f)
                        .fillMaxWidth(),
                state = liste,
                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                items(etat.messages, key = { it.id }) { msg ->
                    BulleMessage(texte = msg.texte, deMoi = msg.deMoi, enCours = msg.enCours) {
                        Row {
                            IconButton(onClick = { viewModel.lecteur.lire(msg.texte) }, modifier = Modifier.size(36.dp)) {
                                Icon(Icons.Filled.VolumeUp, contentDescription = "Lire à voix haute", modifier = Modifier.size(18.dp))
                            }
                            IconButton(onClick = { copier(context, msg.texte) }, modifier = Modifier.size(36.dp)) {
                                Icon(Icons.Filled.ContentCopy, contentDescription = "Copier", modifier = Modifier.size(18.dp))
                            }
                        }
                    }
                }
            }

            if (etat.pret && etat.messages.none { it.deMoi }) {
                LazyRow(
                    contentPadding = PaddingValues(horizontal = 12.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    items(Consignes.suggestions(etat.contexte)) { s ->
                        SuggestionChip(onClick = { viewModel.envoyer(s) }, label = { Text(s) })
                    }
                }
            }

            Row(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .padding(12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                OutlinedTextField(
                    value = saisie,
                    onValueChange = { saisie = it },
                    modifier = Modifier.weight(1f),
                    placeholder = { Text("Votre question…") },
                    shape = RoundedCornerShape(24.dp),
                    maxLines = 4,
                )
                IconButton(onClick = { runCatching { dictee.launch(intentionDictee()) } }) {
                    Icon(Icons.Filled.Mic, contentDescription = "Parler")
                }
                if (etat.reponseEnCours) {
                    FilledIconButton(onClick = { viewModel.arreter() }) {
                        Icon(Icons.Filled.Stop, contentDescription = "Arrêter")
                    }
                } else {
                    FilledIconButton(
                        onClick = {
                            viewModel.envoyer(saisie)
                            saisie = ""
                        },
                        enabled = etat.pret && saisie.isNotBlank(),
                    ) {
                        Icon(Icons.AutoMirrored.Filled.Send, contentDescription = "Envoyer")
                    }
                }
            }
        }
    }

    DisposableEffect(Unit) {
        onDispose { viewModel.lecteur.arreter() }
    }
}

@Composable
private fun BandeauContexte(contexte: ContexteJeu) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 6.dp)
                .clip(RoundedCornerShape(14.dp))
                .background(MaterialTheme.colorScheme.surfaceContainerHigh)
                .padding(8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val apercu =
            remember(contexte.image) {
                contexte.image?.let { runCatching { BitmapFactory.decodeByteArray(it, 0, it.size)?.asImageBitmap() }.getOrNull() }
            }
        if (apercu != null) {
            Image(
                bitmap = apercu,
                contentDescription = "Écran du jeu",
                contentScale = ContentScale.Crop,
                modifier =
                    Modifier
                        .width(72.dp)
                        .height(48.dp)
                        .clip(RoundedCornerShape(8.dp)),
            )
        }
        Column(modifier = Modifier.padding(start = 10.dp)) {
            Text(
                text = contexte.titre ?: "Jeu en cours",
                style = MaterialTheme.typography.titleSmall,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text =
                    listOfNotNull(
                        contexte.console,
                        if (contexte.texteEcran != null) "texte de l'écran lu" else null,
                    ).joinToString(" • "),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

fun descriptionMoteur(etat: MoteurIA.Etat): String =
    when (etat) {
        is MoteurIA.Etat.Pret ->
            etat.modele.nom + (if (etat.gpu) " • puce graphique" else " • processeur") +
                (if (etat.vision) " • voit les images" else "")
        is MoteurIA.Etat.Chargement -> "Chargement…"
        is MoteurIA.Etat.AucunModele -> "Aucun modèle"
        is MoteurIA.Etat.Erreur -> "Erreur"
        MoteurIA.Etat.EnVeille -> "En veille"
    }

private fun copier(
    context: Context,
    texte: String,
) {
    val presse = context.getSystemService(ClipboardManager::class.java)
    presse?.setPrimaryClip(ClipData.newPlainText("Arcadia", texte))
    Toast.makeText(context, "Copié", Toast.LENGTH_SHORT).show()
}
