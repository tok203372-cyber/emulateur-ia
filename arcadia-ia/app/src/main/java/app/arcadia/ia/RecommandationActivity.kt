package app.arcadia.ia

import android.app.Application
import android.content.Intent
import android.os.Bundle
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
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.text.Normalizer

class RecommandationViewModel(application: Application) : AndroidViewModel(application) {
    data class Jeu(val cle: String, val titre: String, val console: String)

    data class Proposition(val jeu: Jeu, val raison: String)

    data class Etat(
        val reflexion: Boolean = false,
        val propositions: List<Proposition> = emptyList(),
        val message: String? = null,
    )

    private val app = application
    private val etatInterne = MutableStateFlow(Etat())
    val etat: StateFlow<Etat> = etatInterne
    val moteur: StateFlow<MoteurIA.Etat> = MoteurIA.etat
    var jeux: List<Jeu> = emptyList()

    fun proposer(envie: String) {
        if (envie.isBlank() || etatInterne.value.reflexion) return
        if (jeux.isEmpty()) {
            etatInterne.value = Etat(message = "Aucun jeu reçu d'Arcadia.")
            return
        }
        etatInterne.value = Etat(reflexion = true)
        viewModelScope.launch {
            val candidats = jeux.take(150)
            val conv = MoteurIA.nouvelleConversation(app, Consignes.CONSIGNE_RECOMMANDATION)
            if (conv == null) {
                etatInterne.value = Etat(message = "Le modèle d'IA n'est pas prêt (voir l'écran d'accueil d'Arcadia IA).")
                return@launch
            }
            val reponse = StringBuilder()
            val reussi =
                runCatching {
                    MoteurIA.repondre(conv, Consignes.recommandation(envie, candidats.map { it.titre to it.console }))
                        .collect { reponse.append(it) }
                }.isSuccess
            val propositions = analyser(reponse.toString(), candidats)
            etatInterne.update {
                Etat(
                    propositions = propositions,
                    message =
                        when {
                            propositions.isNotEmpty() -> null
                            !reussi -> "L'IA n'a pas pu répondre. Réessayez."
                            else -> "Je n'ai pas trouvé de jeu adapté. Essayez une autre envie !"
                        },
                )
            }
        }
    }

    private fun analyser(
        texte: String,
        candidats: List<Jeu>,
    ): List<Proposition> {
        val resultats = mutableListOf<Proposition>()
        val motif = Regex("""^\s*[#\-*•]*\s*(\d{1,4})\s*[|:\-–—.)]+\s*(.+)$""")
        texte.lines().forEach { ligne ->
            val m = motif.find(ligne) ?: return@forEach
            val index = m.groupValues[1].toInt() - 1
            val jeu = candidats.getOrNull(index) ?: return@forEach
            if (resultats.none { it.jeu.cle == jeu.cle }) {
                resultats += Proposition(jeu, m.groupValues[2].trim().trim('*', '"', ' '))
            }
        }
        if (resultats.isEmpty()) {
            // Plan B : l'IA a cité des titres sans suivre le format.
            val bas = normaliser(texte)
            candidats.filter { normaliser(it.titre).length > 3 && bas.contains(normaliser(it.titre)) }
                .take(3)
                .forEach { resultats += Proposition(it, "") }
        }
        return resultats.take(3)
    }

    private fun normaliser(s: String): String =
        Normalizer.normalize(s, Normalizer.Form.NFD).replace(Regex("\\p{Mn}+"), "").lowercase()
}

class RecommandationActivity : ComponentActivity() {
    private val viewModel: RecommandationViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        val cles = intent.getStringArrayListExtra(Contrat.EXTRA_JEUX_CLES).orEmpty()
        val titres = intent.getStringArrayListExtra(Contrat.EXTRA_JEUX_TITRES).orEmpty()
        val consoles = intent.getStringArrayListExtra(Contrat.EXTRA_JEUX_CONSOLES).orEmpty()
        viewModel.jeux =
            cles.indices.mapNotNull { i ->
                val titre = titres.getOrNull(i) ?: return@mapNotNull null
                RecommandationViewModel.Jeu(cles[i], titre, consoles.getOrNull(i) ?: "")
            }

        setContent {
            ThemeArcadia {
                EcranRecommandation(
                    viewModel = viewModel,
                    onJouer = { jeu ->
                        setResult(RESULT_OK, Intent().putExtra(Contrat.RESULTAT_CLE_JEU, jeu.cle))
                        finish()
                    },
                    onFermer = { finish() },
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EcranRecommandation(
    viewModel: RecommandationViewModel,
    onJouer: (RecommandationViewModel.Jeu) -> Unit,
    onFermer: () -> Unit,
) {
    val etat by viewModel.etat.collectAsState()
    val moteur by viewModel.moteur.collectAsState()
    var envie by rememberSaveable { mutableStateOf("") }
    val dictee =
        rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { r ->
            resultatDictee(r.data)?.let {
                envie = it
                viewModel.proposer(it)
            }
        }
    val idees =
        listOf(
            "Une course entre amis",
            "Une aventure tranquille",
            "Un jeu court pour 10 minutes",
            "Un jeu de combat",
            "Un jeu qui fait un peu peur",
            "Surprends-moi !",
        )

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Que jouer ?") },
                navigationIcon = {
                    IconButton(onClick = onFermer) { Icon(Icons.Filled.Close, contentDescription = "Fermer") }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
            )
        },
    ) { marges ->
        LazyColumn(
            modifier =
                Modifier
                    .fillMaxSize()
                    .padding(marges)
                    .imePadding(),
            contentPadding = PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            item {
                Banniere(
                    titre = "De quoi avez-vous envie ?",
                    texte = "L'IA choisit pour vous parmi vos ${viewModel.jeux.size} jeux.",
                    icone = Icons.Filled.AutoAwesome,
                )
            }
            item {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = envie,
                        onValueChange = { envie = it },
                        modifier = Modifier.weight(1f),
                        placeholder = { Text("Ex. : un jeu de plateforme coloré") },
                        shape = RoundedCornerShape(24.dp),
                        singleLine = true,
                    )
                    IconButton(onClick = { runCatching { dictee.launch(intentionDictee()) } }) {
                        Icon(Icons.Filled.Mic, contentDescription = "Parler")
                    }
                }
            }
            item {
                LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(idees) { idee ->
                        SuggestionChip(
                            onClick = {
                                envie = idee
                                viewModel.proposer(idee)
                            },
                            label = { Text(idee) },
                        )
                    }
                }
            }
            item {
                Button(
                    onClick = { viewModel.proposer(envie) },
                    enabled = envie.isNotBlank() && !etat.reflexion,
                    modifier = Modifier.fillMaxWidth(),
                ) { Text("Trouver mon jeu") }
            }
            if (etat.reflexion) {
                item {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text(
                            if (moteur is MoteurIA.Etat.Chargement) "Chargement de l'IA…" else "L'IA réfléchit…",
                            style = MaterialTheme.typography.bodyMedium,
                        )
                        LinearProgressIndicator(Modifier.fillMaxWidth())
                    }
                }
            }
            etat.message?.let { m ->
                item { Text(m, color = MaterialTheme.colorScheme.tertiary) }
            }
            items(etat.propositions, key = { it.jeu.cle }) { p ->
                CarteArcadia(titre = p.jeu.titre, icone = Icons.Filled.SportsEsports) {
                    if (p.jeu.console.isNotBlank()) {
                        Text(p.jeu.console, style = MaterialTheme.typography.labelLarge, color = Couleurs.cyan)
                    }
                    if (p.raison.isNotBlank()) {
                        Text(markdownSimple(p.raison), style = MaterialTheme.typography.bodyMedium)
                    }
                    Button(onClick = { onJouer(p.jeu) }) {
                        Icon(Icons.Filled.PlayArrow, contentDescription = null)
                        Text(" Jouer", fontWeight = FontWeight.Bold)
                    }
                }
            }
        }
    }
}
