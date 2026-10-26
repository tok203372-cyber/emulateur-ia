package com.swordfish.lemuroid.app.arcadia.externe

import android.content.Context
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.CreateNewFolder
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AssistChip
import androidx.compose.material3.AssistChipDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.viewModelScope
import com.swordfish.lemuroid.app.arcadia.ui.ArcadiaCouleurs
import com.swordfish.lemuroid.app.arcadia.ui.JaquetteExterne
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class ConsolesPlusViewModel(private val appContext: Context) : ViewModel() {
    class Factory(private val appContext: Context) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T = ConsolesPlusViewModel(appContext) as T
    }

    data class CarteConsole(
        val console: ConsoleExterne,
        val dossier: String?,
        val installes: List<EmulateurExterne>,
        val choisi: EmulateurExterne?,
        val jeux: List<JeuExterne>,
        val analyse: Boolean,
    )

    private val bibliotheque = BibliothequeExterne.obtenir(appContext)
    private val enAnalyse = MutableStateFlow<Set<String>>(emptySet())
    private val versionEtat = MutableStateFlow(0)

    val cartes: StateFlow<List<CarteConsole>> =
        combine(bibliotheque.jeux(), enAnalyse, versionEtat) { jeux, analyses, _ ->
            ConsoleExterne.values().map { console ->
                CarteConsole(
                    console = console,
                    dossier = bibliotheque.nomDossier(console),
                    installes = LanceurExterne.emulateursInstalles(appContext, console),
                    choisi = LanceurExterne.emulateurPourConsole(appContext, console),
                    jeux = jeux.filter { it.consoleId == console.id },
                    analyse = console.id in analyses,
                )
            }
        }.stateIn(viewModelScope, SharingStarted.Eagerly, emptyList())

    init {
        viewModelScope.launch { bibliotheque.charger() }
    }

    /** À appeler au retour dans l'écran (un émulateur vient peut-être d'être installé). */
    fun rafraichir() {
        versionEtat.value = versionEtat.value + 1
    }

    fun definirDossier(
        console: ConsoleExterne,
        uri: Uri,
    ) {
        bibliotheque.definirDossier(console, uri)
        analyser(console)
    }

    fun analyser(console: ConsoleExterne) {
        if (console.id in enAnalyse.value) return
        viewModelScope.launch {
            enAnalyse.value = enAnalyse.value + console.id
            try {
                bibliotheque.analyser(console)
            } finally {
                enAnalyse.value = enAnalyse.value - console.id
                rafraichir()
            }
        }
    }

    fun choisirEmulateur(
        console: ConsoleExterne,
        emulateur: EmulateurExterne,
    ) {
        bibliotheque.definirEmulateurPrefere(console, emulateur.paquet)
        rafraichir()
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConsolesPlusScreen(
    modifier: Modifier = Modifier,
    viewModel: ConsolesPlusViewModel,
    onJouer: (JeuExterne) -> Unit,
) {
    val context = LocalContext.current
    val cartes by viewModel.cartes.collectAsState()
    var consoleDemandee by remember { mutableStateOf<ConsoleExterne?>(null) }

    val choixDossier =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
            val console = consoleDemandee
            if (uri != null && console != null) viewModel.definirDossier(console, uri)
            consoleDemandee = null
        }

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
        item { Banniere() }
        items(cartes, key = { it.console.id }) { carte ->
            CarteConsoleVue(
                carte = carte,
                onChoisirDossier = {
                    consoleDemandee = carte.console
                    choixDossier.launch(null)
                },
                onAnalyser = { viewModel.analyser(carte.console) },
                onInstaller = { LanceurExterne.ouvrirPageInstallation(context, it) },
                onChoisirEmulateur = { viewModel.choisirEmulateur(carte.console, it) },
                onJouer = onJouer,
            )
        }
    }
}

@Composable
private fun Banniere() {
    Box(
        modifier =
            Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(20.dp))
                .background(
                    Brush.linearGradient(listOf(Color(0xFF2B1F5C), Color(0xFF5B2A86), Color(0xFF8B2F6F))),
                )
                .padding(18.dp),
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(
                text = "Encore plus de consoles",
                style = MaterialTheme.typography.titleLarge,
                fontWeight = FontWeight.Bold,
                color = Color.White,
            )
            Text(
                text =
                    "GameCube, Wii, PS2, Dreamcast, Switch… Arcadia range vos jeux et les lance " +
                        "dans l'émulateur spécialisé installé sur votre téléphone. Choisissez un dossier " +
                        "par console : c'est tout.",
                style = MaterialTheme.typography.bodyMedium,
                color = Color.White.copy(alpha = 0.88f),
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun CarteConsoleVue(
    carte: ConsolesPlusViewModel.CarteConsole,
    onChoisirDossier: () -> Unit,
    onAnalyser: () -> Unit,
    onInstaller: (EmulateurExterne) -> Unit,
    onChoisirEmulateur: (EmulateurExterne) -> Unit,
    onJouer: (JeuExterne) -> Unit,
) {
    val console = carte.console
    val couleur = Color(console.couleur)
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
                Box(
                    modifier =
                        Modifier
                            .size(14.dp)
                            .clip(CircleShape)
                            .background(Brush.linearGradient(listOf(couleur, ArcadiaCouleurs.rose))),
                )
                Text(
                    text = console.nom,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                    modifier =
                        Modifier
                            .padding(start = 10.dp)
                            .weight(1f),
                )
                if (carte.jeux.isNotEmpty()) {
                    Text(
                        text = if (carte.jeux.size > 1) "${carte.jeux.size} jeux" else "1 jeu",
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.primary,
                    )
                }
            }

            // Émulateur
            val choisi = carte.choisi
            if (choisi != null) {
                if (carte.installes.size > 1) {
                    LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        items(carte.installes, key = { it.paquet }) { emu ->
                            FilterChip(
                                selected = emu.paquet == choisi.paquet,
                                onClick = { onChoisirEmulateur(emu) },
                                label = { Text(emu.nom) },
                            )
                        }
                    }
                } else {
                    AssistChip(
                        onClick = {},
                        label = { Text("${choisi.nom} est installé") },
                        leadingIcon = {
                            Icon(
                                Icons.Filled.CheckCircle,
                                contentDescription = null,
                                tint = ArcadiaCouleurs.cyan,
                                modifier = Modifier.size(AssistChipDefaults.IconSize),
                            )
                        },
                    )
                }
            } else {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Filled.Warning, contentDescription = null, tint = ArcadiaCouleurs.or)
                    Text(
                        text = "Émulateur à installer : " + console.emulateurs.joinToString(" ou ") { it.nom },
                        style = MaterialTheme.typography.bodyMedium,
                        modifier = Modifier.padding(start = 8.dp),
                    )
                }
                val installable = console.emulateurs.firstOrNull { it.lienInstallation != null }
                if (installable != null) {
                    FilledTonalButton(onClick = { onInstaller(installable) }) {
                        Icon(Icons.Filled.Download, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp))
                        Text("Installer ${installable.nom}")
                    }
                }
            }

            Text(
                text = console.conseil,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            // Dossier des jeux
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                if (carte.dossier == null) {
                    FilledTonalButton(onClick = onChoisirDossier) {
                        Icon(Icons.Filled.CreateNewFolder, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp))
                        Text("Choisir le dossier des jeux")
                    }
                } else {
                    OutlinedButton(onClick = onChoisirDossier) {
                        Icon(Icons.Filled.Folder, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(6.dp))
                        Text(
                            text = carte.dossier,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                            modifier = Modifier.width(140.dp),
                        )
                    }
                    OutlinedButton(onClick = onAnalyser, enabled = !carte.analyse) {
                        Icon(Icons.Filled.Refresh, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(6.dp))
                        Text("Actualiser")
                    }
                }
            }

            if (carte.analyse) {
                LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            }

            if (carte.jeux.isNotEmpty()) {
                LazyRow(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    items(carte.jeux, key = { it.uri }) { jeu ->
                        Column(
                            modifier =
                                Modifier
                                    .width(104.dp)
                                    .clip(RoundedCornerShape(10.dp))
                                    .clickable { onJouer(jeu) },
                        ) {
                            JaquetteExterne(
                                modifier =
                                    Modifier
                                        .width(104.dp)
                                        .height(144.dp),
                                jeu = jeu,
                            )
                            Text(
                                text = jeu.titre,
                                style = MaterialTheme.typography.labelMedium,
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis,
                                textAlign = TextAlign.Start,
                                modifier = Modifier.padding(top = 4.dp, start = 2.dp, end = 2.dp),
                            )
                            if (jeu.etiquette != console.nom) {
                                Text(
                                    text = jeu.etiquette,
                                    style = MaterialTheme.typography.labelSmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    modifier = Modifier.padding(start = 2.dp, bottom = 4.dp),
                                )
                            }
                        }
                    }
                }
            } else if (carte.dossier != null && !carte.analyse) {
                Text(
                    text = "Aucun jeu trouvé dans ce dossier. Vérifiez qu'il contient des fichiers " +
                        console.extensions.sorted().joinToString(", ") { ".$it" } + ".",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
