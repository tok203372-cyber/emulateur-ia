package com.swordfish.lemuroid.app.arcadia.changer

import android.content.Context
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Clear
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.ViewList
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.swordfish.lemuroid.app.arcadia.ProchainJeu
import com.swordfish.lemuroid.app.arcadia.ui.JaquetteExterne
import com.swordfish.lemuroid.app.mobile.shared.compose.ui.LemuroidSmallGameImage
import com.swordfish.lemuroid.lib.library.db.RetrogradeDatabase
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class ChangerDeJeuViewModel(
    private val appContext: Context,
    private val base: RetrogradeDatabase,
    private val idJeuActuel: Int,
) : ViewModel() {
    class Factory(
        private val appContext: Context,
        private val base: RetrogradeDatabase,
        private val idJeuActuel: Int,
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T =
            ChangerDeJeuViewModel(appContext, base, idJeuActuel) as T
    }

    data class Filtre(val id: String, val nom: String)

    data class Etat(
        val chargement: Boolean = true,
        val filtres: List<Filtre> = emptyList(),
        val filtre: String = FILTRE_RECENTS,
        val recherche: String = "",
        val elements: List<ElementJeu> = emptyList(),
    )

    private val tous = MutableStateFlow<List<ElementJeu>?>(null)
    private val filtre = MutableStateFlow(FILTRE_RECENTS)
    private val recherche = MutableStateFlow("")

    val etat: StateFlow<Etat> =
        combine(tous, filtre, recherche) { liste, f, r ->
            if (liste == null) return@combine Etat(chargement = true, filtre = f, recherche = r)
            val filtres =
                listOf(
                    Filtre(FILTRE_RECENTS, "Récents"),
                    Filtre(FILTRE_FAVORIS, "Favoris"),
                    Filtre(FILTRE_TOUS, "Tous"),
                ) + CatalogueJeux.groupes(liste).map { Filtre(it.id, "${it.nom} (${it.nombre})") }

            val visibles =
                if (r.isNotBlank()) {
                    CatalogueJeux.rechercher(liste, r)
                } else {
                    when (f) {
                        FILTRE_RECENTS ->
                            liste.filter { it.dernierePartie != null }
                                .sortedByDescending { it.dernierePartie }
                                .take(40)
                                .ifEmpty { liste }
                        FILTRE_FAVORIS -> liste.filter { it.favori }
                        FILTRE_TOUS -> liste
                        else -> liste.filter { it.groupe == f }
                    }
                }
            Etat(false, filtres, f, r, visibles)
        }.stateIn(viewModelScope, SharingStarted.Eagerly, Etat())

    init {
        viewModelScope.launch {
            tous.value = CatalogueJeux.charger(appContext, base, idJeuActuel)
        }
    }

    fun choisirFiltre(id: String) {
        filtre.value = id
        recherche.value = ""
    }

    fun rechercher(texte: String) {
        recherche.value = texte
    }

    companion object {
        const val FILTRE_RECENTS = "recents"
        const val FILTRE_FAVORIS = "favoris"
        const val FILTRE_TOUS = "tous"
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ChangerDeJeuScreen(
    viewModel: ChangerDeJeuViewModel,
    onChoisir: (ProchainJeu) -> Unit,
) {
    val etat by viewModel.etat.collectAsState()
    val focus = LocalFocusManager.current

    Column(modifier = Modifier.fillMaxSize()) {
        Text(
            text = "La partie en cours est sauvegardée, puis le jeu choisi démarre avec le bon émulateur.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 8.dp),
        )

        OutlinedTextField(
            value = etat.recherche,
            onValueChange = { viewModel.rechercher(it) },
            modifier =
                Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 8.dp),
            singleLine = true,
            placeholder = { Text("Rechercher un jeu…") },
            leadingIcon = { Icon(Icons.Filled.Search, contentDescription = null) },
            trailingIcon = {
                if (etat.recherche.isNotEmpty()) {
                    IconButton(onClick = { viewModel.rechercher("") }) {
                        Icon(Icons.Filled.Clear, contentDescription = "Effacer")
                    }
                }
            },
            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
            keyboardActions = KeyboardActions(onSearch = { focus.clearFocus() }),
        )

        LazyRow(
            contentPadding = PaddingValues(horizontal = 16.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            items(etat.filtres, key = { it.id }) { f ->
                val icone: ImageVector? =
                    when (f.id) {
                        ChangerDeJeuViewModel.FILTRE_RECENTS -> Icons.Filled.History
                        ChangerDeJeuViewModel.FILTRE_FAVORIS -> Icons.Filled.Favorite
                        ChangerDeJeuViewModel.FILTRE_TOUS -> Icons.Filled.ViewList
                        else -> null
                    }
                FilterChip(
                    selected = etat.recherche.isBlank() && etat.filtre == f.id,
                    onClick = { viewModel.choisirFiltre(f.id) },
                    label = { Text(f.nom) },
                    leadingIcon =
                        if (icone != null) {
                            {
                                Icon(icone, contentDescription = null, modifier = Modifier.size(18.dp))
                            }
                        } else {
                            null
                        },
                )
            }
        }

        when {
            etat.chargement ->
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator()
                }
            etat.elements.isEmpty() ->
                Box(Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
                    Text(
                        text =
                            if (etat.recherche.isNotBlank()) {
                                "Aucun jeu ne correspond à « ${etat.recherche} »."
                            } else {
                                "Aucun jeu ici pour l'instant."
                            },
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            else ->
                LazyColumn(
                    modifier = Modifier.fillMaxSize(),
                    contentPadding = PaddingValues(vertical = 8.dp),
                ) {
                    items(etat.elements, key = { it.cle }) { element ->
                        LigneJeu(element) { onChoisir(element.versProchain()) }
                    }
                }
        }
    }
}

@Composable
private fun LigneJeu(
    element: ElementJeu,
    onClick: () -> Unit,
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .clickable(onClick = onClick)
                .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        val jeuInterne = element.interne
        val jeuExterne = element.externe
        if (jeuInterne != null) {
            LemuroidSmallGameImage(modifier = Modifier.size(44.dp), game = jeuInterne)
        } else if (jeuExterne != null) {
            JaquetteExterne(modifier = Modifier.size(44.dp), jeu = jeuExterne, compacte = true)
        }
        Column(
            modifier =
                Modifier
                    .weight(1f)
                    .padding(start = 12.dp),
        ) {
            Text(
                text = element.titre,
                style = MaterialTheme.typography.bodyLarge,
                fontWeight = FontWeight.Medium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = if (jeuExterne != null) "${element.console} • émulateur externe" else element.console,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        Icon(
            Icons.Filled.PlayArrow,
            contentDescription = "Jouer",
            tint = MaterialTheme.colorScheme.primary,
        )
    }
}
