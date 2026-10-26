package com.swordfish.lemuroid.app.arcadia.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.SubcomposeAsyncImage
import com.swordfish.lemuroid.app.arcadia.externe.ConsoleExterne
import com.swordfish.lemuroid.app.arcadia.externe.JeuExterne
import java.text.Normalizer
import java.util.Locale

/** Couleurs d'Arcadia, communes à tous les écrans ajoutés. */
object ArcadiaCouleurs {
    val violet = Color(0xFF8B6CFF)
    val rose = Color(0xFFFF5FA2)
    val cyan = Color(0xFF4FE3D0)
    val or = Color(0xFFFFD166)
    val nuit = Color(0xFF120E22)

    val degradeIA = Brush.linearGradient(listOf(Color(0xFF6A4CFF), Color(0xFFB04CFF), Color(0xFFFF5FA2)))
}

/** Texte sans accents ni majuscules, pour les recherches (« Pokémon » = « pokemon »). */
fun normaliser(texte: String): String =
    Normalizer.normalize(texte, Normalizer.Form.NFD)
        .replace(Regex("\\p{Mn}+"), "")
        .lowercase(Locale.ROOT)
        .replace(Regex("[^a-z0-9 ]"), " ")
        .replace(Regex("\\s+"), " ")
        .trim()

/** Tuile colorée quand un jeu n'a pas de jaquette. */
@Composable
fun TuileConsole(
    modifier: Modifier = Modifier,
    couleur: Long,
    nomConsole: String,
    titre: String,
    compacte: Boolean = false,
) {
    val base = Color(couleur)
    Box(
        modifier =
            modifier
                .clip(MaterialTheme.shapes.small)
                .background(
                    Brush.linearGradient(
                        listOf(base, Color(0xFF120E22)),
                    ),
                ),
        contentAlignment = Alignment.Center,
    ) {
        if (compacte) {
            Text(
                text = initiales(titre),
                color = Color.White,
                fontWeight = FontWeight.Bold,
                fontSize = 14.sp,
            )
        } else {
            Column(
                modifier = Modifier.padding(8.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text(
                    text = titre,
                    color = Color.White,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center,
                    maxLines = 4,
                    overflow = TextOverflow.Ellipsis,
                    style = MaterialTheme.typography.labelLarge,
                )
                Text(
                    text = nomConsole,
                    color = Color.White.copy(alpha = 0.75f),
                    textAlign = TextAlign.Center,
                    style = MaterialTheme.typography.labelSmall,
                    modifier = Modifier.padding(top = 4.dp),
                )
            }
        }
    }
}

private fun initiales(titre: String): String =
    titre.split(' ', '-', ':')
        .filter { it.isNotBlank() && it[0].isLetterOrDigit() }
        .take(2)
        .joinToString("") { it.take(1).uppercase(Locale.ROOT) }
        .ifEmpty { "?" }

/** Jaquette d'un jeu externe (GameTDB pour GameCube/Wii), ou tuile colorée à défaut. */
@Composable
fun JaquetteExterne(
    modifier: Modifier = Modifier,
    jeu: JeuExterne,
    compacte: Boolean = false,
) {
    val console = jeu.console ?: ConsoleExterne.GAMECUBE_WII
    if (jeu.jaquette == null) {
        TuileConsole(modifier, console.couleur, jeu.etiquette, jeu.titre, compacte)
        return
    }
    SubcomposeAsyncImage(
        model = jeu.jaquette,
        contentDescription = jeu.titre,
        modifier = modifier.clip(MaterialTheme.shapes.small),
        contentScale = ContentScale.Crop,
        loading = { TuileConsole(Modifier.fillMaxSize(), console.couleur, jeu.etiquette, jeu.titre, compacte) },
        error = { TuileConsole(Modifier.fillMaxSize(), console.couleur, jeu.etiquette, jeu.titre, compacte) },
    )
}
