package com.swordfish.lemuroid.app.mobile.feature.main

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.fredporciuncula.flow.preferences.FlowSharedPreferences
import com.swordfish.lemuroid.R
import android.widget.Toast
import androidx.lifecycle.lifecycleScope
import com.swordfish.lemuroid.app.arcadia.Enchainement
import com.swordfish.lemuroid.app.arcadia.ProchainJeu
import com.swordfish.lemuroid.app.arcadia.changer.CatalogueJeux
import com.swordfish.lemuroid.app.arcadia.changer.ElementJeu
import com.swordfish.lemuroid.app.arcadia.externe.BibliothequeExterne
import com.swordfish.lemuroid.app.arcadia.externe.ConsolesPlusScreen
import com.swordfish.lemuroid.app.arcadia.externe.ConsolesPlusViewModel
import com.swordfish.lemuroid.app.arcadia.externe.JeuExterne
import com.swordfish.lemuroid.app.arcadia.externe.LanceurExterne
import com.swordfish.lemuroid.app.arcadia.ia.ArcadiaIAScreen
import com.swordfish.lemuroid.app.arcadia.ia.ArcadiaIAViewModel
import com.swordfish.lemuroid.app.arcadia.ia.CompagnonIA
import com.swordfish.lemuroid.lib.library.GameSystem
import kotlinx.coroutines.launch
import com.swordfish.lemuroid.app.mobile.feature.favorites.FavoritesScreen
import com.swordfish.lemuroid.app.mobile.feature.favorites.FavoritesViewModel
import com.swordfish.lemuroid.app.mobile.feature.games.GamesScreen
import com.swordfish.lemuroid.app.mobile.feature.games.GamesViewModel
import com.swordfish.lemuroid.app.mobile.feature.home.HomeScreen
import com.swordfish.lemuroid.app.mobile.feature.home.HomeViewModel
import com.swordfish.lemuroid.app.mobile.feature.search.SearchScreen
import com.swordfish.lemuroid.app.mobile.feature.search.SearchViewModel
import com.swordfish.lemuroid.app.mobile.feature.settings.advanced.AdvancedSettingsScreen
import com.swordfish.lemuroid.app.mobile.feature.settings.advanced.AdvancedSettingsViewModel
import com.swordfish.lemuroid.app.mobile.feature.settings.bios.BiosScreen
import com.swordfish.lemuroid.app.mobile.feature.settings.bios.BiosSettingsViewModel
import com.swordfish.lemuroid.app.mobile.feature.settings.coreselection.CoresSelectionScreen
import com.swordfish.lemuroid.app.mobile.feature.settings.coreselection.CoresSelectionViewModel
import com.swordfish.lemuroid.app.mobile.feature.settings.general.SettingsScreen
import com.swordfish.lemuroid.app.mobile.feature.settings.general.SettingsViewModel
import com.swordfish.lemuroid.app.mobile.feature.settings.inputdevices.InputDevicesSettingsScreen
import com.swordfish.lemuroid.app.mobile.feature.settings.inputdevices.InputDevicesSettingsViewModel
import com.swordfish.lemuroid.app.mobile.feature.settings.savesync.SaveSyncSettingsScreen
import com.swordfish.lemuroid.app.mobile.feature.settings.savesync.SaveSyncSettingsViewModel
import com.swordfish.lemuroid.app.mobile.feature.shortcuts.ShortcutsGenerator
import com.swordfish.lemuroid.app.mobile.feature.systems.MetaSystemsScreen
import com.swordfish.lemuroid.app.mobile.feature.systems.MetaSystemsViewModel
import com.swordfish.lemuroid.app.mobile.shared.compose.ui.AppTheme
import com.swordfish.lemuroid.app.shared.GameInteractor
import com.swordfish.lemuroid.app.shared.game.BaseGameActivity
import com.swordfish.lemuroid.app.shared.game.GameLauncher
import com.swordfish.lemuroid.app.shared.input.InputDeviceManager
import com.swordfish.lemuroid.app.shared.main.BusyActivity
import com.swordfish.lemuroid.app.shared.main.GameLaunchTaskHandler
import com.swordfish.lemuroid.app.shared.settings.SettingsInteractor
import com.swordfish.lemuroid.common.coroutines.safeLaunch
import com.swordfish.lemuroid.ext.feature.review.ReviewManager
import com.swordfish.lemuroid.lib.android.RetrogradeComponentActivity
import com.swordfish.lemuroid.lib.bios.BiosManager
import com.swordfish.lemuroid.lib.core.CoresSelection
import com.swordfish.lemuroid.lib.injection.PerActivity
import com.swordfish.lemuroid.lib.library.covers.CustomCoverManager
import com.swordfish.lemuroid.lib.library.MetaSystemID
import com.swordfish.lemuroid.lib.library.SystemID
import com.swordfish.lemuroid.lib.library.db.RetrogradeDatabase
import com.swordfish.lemuroid.lib.library.db.entity.Game
import com.swordfish.lemuroid.lib.preferences.SharedPreferencesHelper
import com.swordfish.lemuroid.lib.savesync.SaveSyncManager
import com.swordfish.lemuroid.lib.storage.DirectoriesManager
import dagger.Provides
import de.charlex.compose.material3.HtmlText
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import javax.inject.Inject

@OptIn(DelicateCoroutinesApi::class)
class MainActivity : RetrogradeComponentActivity(), BusyActivity {
    @Inject
    lateinit var gameLaunchTaskHandler: GameLaunchTaskHandler

    @Inject
    lateinit var saveSyncManager: SaveSyncManager

    @Inject
    lateinit var retrogradeDb: RetrogradeDatabase

    @Inject
    lateinit var gameInteractor: GameInteractor

    @Inject
    lateinit var biosManager: BiosManager

    @Inject
    lateinit var coresSelection: CoresSelection

    @Inject
    lateinit var settingsInteractor: SettingsInteractor

    @Inject
    lateinit var inputDeviceManager: InputDeviceManager

    // Arcadia : lancement direct du jeu suivant (« Changer de jeu »).
    @Inject
    lateinit var gameLauncher: GameLauncher

    private val reviewManager = ReviewManager()

    private val mainViewModel: MainViewModel by viewModels {
        MainViewModel.Factory(applicationContext, saveSyncManager)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge(
            SystemBarStyle.dark(Color.TRANSPARENT),
            SystemBarStyle.dark(Color.TRANSPARENT),
        )
        super.onCreate(savedInstanceState)

        GlobalScope.safeLaunch {
            reviewManager.initialize(applicationContext)
        }

        setContent {
            val navController = rememberNavController()
            MainScreen(navController)
        }
    }

    @OptIn(ExperimentalMaterial3Api::class)
    @Composable
    private fun MainScreen(navController: NavHostController) {
        AppTheme {
            val navBackStackEntry = navController.currentBackStackEntryAsState()
            val currentDestination = navBackStackEntry.value?.destination
            val currentRoute =
                currentDestination?.route
                    ?.let { MainRoute.findByRoute(it) }
                    ?: MainRoute.HOME

            val infoDialogDisplayed =
                remember {
                    mutableStateOf(false)
                }

            LaunchedEffect(currentRoute) {
                mainViewModel.changeRoute(currentRoute)
            }

            val selectedGameState =
                remember {
                    mutableStateOf<Game?>(null)
                }

            val onGameLongClick = { game: Game ->
                selectedGameState.value = game
            }

            val onGameClick = { game: Game ->
                gameInteractor.onGamePlay(game)
            }

            val onGameFavoriteToggle = { game: Game, isFavorite: Boolean ->
                gameInteractor.onFavoriteToggle(game, isFavorite)
            }

            val onHelpPressed = {
                infoDialogDisplayed.value = true
            }

            val mainUIState =
                mainViewModel.state
                    .collectAsState(MainViewModel.UiState())
                    .value

            Scaffold(
                topBar = {
                    MainTopBar(
                        currentRoute = currentRoute,
                        navController = navController,
                        onHelpPressed = onHelpPressed,
                        mainUIState = mainUIState,
                        onUpdateQueryString = { mainViewModel.changeQueryString(it) },
                    )
                },
                bottomBar = { MainNavigationBar(currentRoute, navController) },
            ) { padding ->
                NavHost(
                    modifier = Modifier.fillMaxSize(),
                    navController = navController,
                    startDestination = MainRoute.HOME.route,
                ) {
                    composable(MainRoute.HOME) {
                        HomeScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory =
                                        HomeViewModel.Factory(
                                            applicationContext,
                                            retrogradeDb,
                                            coresSelection,
                                        ),
                                ),
                            onGameClick = onGameClick,
                            onGameLongClick = onGameLongClick,
                            onOpenCoreSelection = { navController.navigateToRoute(MainRoute.SETTINGS_CORES_SELECTION) },
                        )
                    }
                    composable(MainRoute.FAVORITES) {
                        FavoritesScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory = FavoritesViewModel.Factory(retrogradeDb),
                                ),
                            onGameClick = onGameClick,
                            onGameLongClick = onGameLongClick,
                        )
                    }
                    composable(MainRoute.SEARCH) {
                        SearchScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory = SearchViewModel.Factory(retrogradeDb),
                                ),
                            searchQuery = mainUIState.searchQuery,
                            onGameClick = onGameClick,
                            onGameLongClick = onGameLongClick,
                            onGameFavoriteToggle = onGameFavoriteToggle,
                            onResetSearchQuery = { mainViewModel.changeQueryString("") },
                        )
                    }
                    composable(MainRoute.SYSTEMS) {
                        MetaSystemsScreen(
                            modifier = Modifier.padding(padding),
                            navController = navController,
                            viewModel =
                                viewModel(
                                    factory =
                                        MetaSystemsViewModel.Factory(
                                            retrogradeDb,
                                            applicationContext,
                                        ),
                                ),
                        )
                    }
                    composable(MainRoute.SYSTEM_GAMES) { entry ->
                        val metaSystemId = entry.arguments?.getString("metaSystemId")
                        GamesScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory =
                                        GamesViewModel.Factory(
                                            retrogradeDb,
                                            MetaSystemID.valueOf(metaSystemId!!),
                                        ),
                                ),
                            onGameClick = onGameClick,
                            onGameLongClick = onGameLongClick,
                            onGameFavoriteToggle = onGameFavoriteToggle,
                        )
                    }
                    composable(MainRoute.PLUS) {
                        ConsolesPlusScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory = ConsolesPlusViewModel.Factory(applicationContext),
                                ),
                            onJouer = { jeu -> lancerJeuExterne(jeu) },
                        )
                    }
                    composable(MainRoute.IA) {
                        ArcadiaIAScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory = ArcadiaIAViewModel.Factory(applicationContext),
                                ),
                            onDiscuter = { CompagnonIA.ouvrirAccueil(this@MainActivity) },
                            onQueJouer = { demanderRecommandation() },
                            onReglagesManettes = { navController.navigateToRoute(MainRoute.SETTINGS_INPUT_DEVICES) },
                        )
                    }
                    composable(MainRoute.SETTINGS) {
                        SettingsScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory =
                                        SettingsViewModel.Factory(
                                            applicationContext,
                                            settingsInteractor,
                                            saveSyncManager,
                                            FlowSharedPreferences(
                                                SharedPreferencesHelper.getLegacySharedPreferences(
                                                    applicationContext,
                                                ),
                                            ),
                                        ),
                                ),
                            navController = navController,
                        )
                    }
                    composable(MainRoute.SETTINGS_ADVANCED) {
                        AdvancedSettingsScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory =
                                        AdvancedSettingsViewModel.Factory(
                                            applicationContext,
                                            settingsInteractor,
                                        ),
                                ),
                            navController = navController,
                        )
                    }
                    composable(MainRoute.SETTINGS_BIOS) {
                        BiosScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory = BiosSettingsViewModel.Factory(biosManager),
                                ),
                        )
                    }
                    composable(MainRoute.SETTINGS_CORES_SELECTION) {
                        CoresSelectionScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory =
                                        CoresSelectionViewModel.Factory(
                                            applicationContext,
                                            coresSelection,
                                        ),
                                ),
                        )
                    }
                    composable(MainRoute.SETTINGS_INPUT_DEVICES) {
                        InputDevicesSettingsScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory =
                                        InputDevicesSettingsViewModel.Factory(
                                            applicationContext,
                                            inputDeviceManager,
                                        ),
                                ),
                        )
                    }
                    composable(MainRoute.SETTINGS_SAVE_SYNC) {
                        SaveSyncSettingsScreen(
                            modifier = Modifier.padding(padding),
                            viewModel =
                                viewModel(
                                    factory =
                                        SaveSyncSettingsViewModel.Factory(
                                            application,
                                            saveSyncManager,
                                        ),
                                ),
                        )
                    }
                }
            }

            MainGameContextActions(
                selectedGameState = selectedGameState,
                shortcutSupported = gameInteractor.supportShortcuts(),
                onGamePlay = { gameInteractor.onGamePlay(it) },
                onGameRestart = { gameInteractor.onGameRestart(it) },
                onFavoriteToggle = { game: Game, isFavorite: Boolean ->
                    gameInteractor.onFavoriteToggle(game, isFavorite)
                },
                onCreateShortcut = { gameInteractor.onCreateShortcut(it) },
                onSetCustomName = { game, name -> gameInteractor.onSetCustomName(game, name) },
                onClearCustomName = { game -> gameInteractor.onClearCustomName(game) },
                onSetCustomCoverUri = { game, uri -> gameInteractor.onSetCustomCoverUri(game, uri) },
                onClearCustomCoverUri = { game -> gameInteractor.onClearCustomCoverUri(game) },
                onAskAI = { game -> presenterJeu(game) },
            )

            if (infoDialogDisplayed.value) {
                val message =
                    remember {
                        val systemFolders =
                            SystemID.values()
                                .joinToString(", ") { "<i>${it.dbname}</i>" }

                        getString(R.string.lemuroid_help_content)
                            .replace("\$SYSTEMS", systemFolders)
                    }

                AlertDialog(
                    text = { HtmlText(text = message) },
                    onDismissRequest = { infoDialogDisplayed.value = false },
                    confirmButton = { },
                )
            }
        }
    }

    override fun activity(): Activity = this

    override fun isBusy(): Boolean = mainViewModel.state.value.operationInProgress ?: false

    override fun onActivityResult(
        requestCode: Int,
        resultCode: Int,
        data: Intent?,
    ) {
        super.onActivityResult(requestCode, resultCode, data)

        when (requestCode) {
            BaseGameActivity.REQUEST_PLAY_GAME -> {
                GlobalScope.safeLaunch {
                    gameLaunchTaskHandler.handleGameFinish(
                        true,
                        this@MainActivity,
                        resultCode,
                        data,
                    )
                }
                // Arcadia : « Changer de jeu » depuis le menu en jeu.
                val prochain = ProchainJeu.depuis(data)
                if (prochain != null && resultCode == Activity.RESULT_OK) {
                    lifecycleScope.launch {
                        Enchainement.lancer(this@MainActivity, prochain, retrogradeDb) { jeu ->
                            gameLauncher.launchGameAsync(this@MainActivity, jeu, true, false)
                        }
                    }
                }
            }
            CompagnonIA.DEMANDE_RECOMMANDATION -> {
                if (resultCode == Activity.RESULT_OK) {
                    lancerDepuisCle(data?.getStringExtra(CompagnonIA.RESULTAT_CLE_JEU))
                }
            }
        }
    }

    // ----- Arcadia -----

    private fun lancerJeuExterne(jeu: JeuExterne) {
        val resultat = LanceurExterne.lancer(this, jeu)
        if (resultat is LanceurExterne.Resultat.Lance) {
            lifecycleScope.launch { BibliothequeExterne.obtenir(applicationContext).noterPartie(jeu) }
        } else {
            LanceurExterne.expliquer(this, resultat)
        }
    }

    private fun demanderRecommandation() {
        lifecycleScope.launch {
            val jeux = CatalogueJeux.charger(applicationContext, retrogradeDb)
            if (jeux.isEmpty()) {
                Toast.makeText(
                    this@MainActivity,
                    "Ajoutez d'abord des jeux : l'IA choisira parmi eux.",
                    Toast.LENGTH_LONG,
                ).show()
                return@launch
            }
            // Les favoris et les jeux joués récemment d'abord : l'IA ne lit qu'une partie de la liste.
            val ordonnes =
                jeux.sortedWith(
                    compareByDescending<ElementJeu> { it.favori }.thenByDescending { it.dernierePartie ?: 0L },
                )
            CompagnonIA.demanderRecommandation(
                this@MainActivity,
                ordonnes.map { CompagnonIA.JeuPropose(it.cle, it.titre, it.console) },
            )
        }
    }

    private fun lancerDepuisCle(cle: String?) {
        if (cle == null) return
        lifecycleScope.launch {
            when {
                cle.startsWith("i:") -> {
                    val id = cle.removePrefix("i:").toIntOrNull() ?: return@launch
                    val jeu = retrogradeDb.gameDao().selectById(id) ?: return@launch
                    gameInteractor.onGamePlay(jeu)
                }
                cle.startsWith("e:") -> {
                    val uri = cle.removePrefix("e:")
                    val jeu =
                        BibliothequeExterne.obtenir(applicationContext).charger().firstOrNull { it.uri == uri }
                            ?: return@launch
                    lancerJeuExterne(jeu)
                }
            }
        }
    }

    private fun presenterJeu(game: Game) {
        val console = runCatching { getString(GameSystem.findById(game.systemId).titleResId) }.getOrNull()
        CompagnonIA.ouvrirAssistant(
            activity = this,
            titreJeu = game.displayName(),
            console = console,
            question =
                "Présente-moi ce jeu en quelques phrases : de quoi il parle, comment on y joue, " +
                    "et donne-moi 3 conseils pour bien débuter.",
        )
    }

    @dagger.Module
    abstract class Module {
        @dagger.Module
        companion object {
            @Provides
            @PerActivity
            @JvmStatic
            fun settingsInteractor(
                activity: MainActivity,
                directoriesManager: DirectoriesManager,
            ) = SettingsInteractor(activity, directoriesManager)

            @Provides
            @PerActivity
            @JvmStatic
            fun gameInteractor(
                activity: MainActivity,
                retrogradeDb: RetrogradeDatabase,
                shortcutsGenerator: ShortcutsGenerator,
                gameLauncher: GameLauncher,
                customCoverManager: CustomCoverManager,
            ) = GameInteractor(activity, retrogradeDb, false, shortcutsGenerator, gameLauncher, customCoverManager)
        }
    }
}
