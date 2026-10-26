package app.arcadia.ia

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp

object Couleurs {
    val violet = Color(0xFF8B6CFF)
    val violetClair = Color(0xFFB9A3FF)
    val rose = Color(0xFFFF5FA2)
    val cyan = Color(0xFF4FE3D0)
    val or = Color(0xFFFFD166)
    val nuit = Color(0xFF0F0B1C)
    val carte = Color(0xFF1A1430)
    val carteHaute = Color(0xFF231C3B)
    val degrade = Brush.linearGradient(listOf(Color(0xFF6A4CFF), Color(0xFFB04CFF), Color(0xFFFF5FA2)))
}

private val schema =
    darkColorScheme(
        primary = Couleurs.violetClair,
        onPrimary = Color(0xFF2A1070),
        primaryContainer = Color(0xFF4B2CB0),
        onPrimaryContainer = Color(0xFFE9DDFF),
        secondary = Color(0xFF5EE6D2),
        onSecondary = Color(0xFF003731),
        secondaryContainer = Color(0xFF005048),
        onSecondaryContainer = Color(0xFF7FF8E4),
        tertiary = Color(0xFFFF8FC4),
        onTertiary = Color(0xFF5C1138),
        background = Couleurs.nuit,
        onBackground = Color(0xFFE8E2F4),
        surface = Couleurs.nuit,
        onSurface = Color(0xFFE8E2F4),
        surfaceVariant = Color(0xFF2A2442),
        onSurfaceVariant = Color(0xFFCAC2DE),
        surfaceContainer = Couleurs.carte,
        surfaceContainerHigh = Couleurs.carteHaute,
        surfaceContainerHighest = Color(0xFF2D2548),
        surfaceContainerLow = Color(0xFF151027),
        surfaceContainerLowest = Color(0xFF0A0715),
        outline = Color(0xFF938BA8),
        outlineVariant = Color(0xFF48415E),
    )

@Composable
fun ThemeArcadia(contenu: @Composable () -> Unit) {
    MaterialTheme(colorScheme = schema, content = contenu)
}

@Composable
fun CarteArcadia(
    modifier: Modifier = Modifier,
    titre: String? = null,
    icone: ImageVector? = null,
    contenu: @Composable () -> Unit,
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = Couleurs.carte),
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            if (titre != null) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    if (icone != null) {
                        Icon(icone, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                    }
                    Text(
                        text = if (icone != null) "  $titre" else titre,
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.Bold,
                    )
                }
            }
            contenu()
        }
    }
}

@Composable
fun Banniere(
    titre: String,
    texte: String,
    icone: ImageVector,
) {
    Box(
        modifier =
            Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(24.dp))
                .background(Couleurs.degrade)
                .padding(20.dp),
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(icone, contentDescription = null, tint = Color.White)
                Text(
                    text = "  $titre",
                    style = MaterialTheme.typography.headlineSmall,
                    fontWeight = FontWeight.Bold,
                    color = Color.White,
                )
            }
            Text(texte, style = MaterialTheme.typography.bodyMedium, color = Color.White.copy(alpha = 0.92f))
        }
    }
}

/** Bulle de discussion : à droite pour l'utilisateur, à gauche (bordure dégradée) pour l'IA. */
@Composable
fun BulleMessage(
    texte: String,
    deMoi: Boolean,
    enCours: Boolean,
    actions: @Composable () -> Unit = {},
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = if (deMoi) Arrangement.End else Arrangement.Start,
    ) {
        val forme =
            if (deMoi) RoundedCornerShape(20.dp, 20.dp, 6.dp, 20.dp) else RoundedCornerShape(20.dp, 20.dp, 20.dp, 6.dp)
        Column(
            modifier =
                Modifier
                    .widthIn(max = 330.dp)
                    .clip(forme)
                    .then(
                        if (deMoi) {
                            Modifier.background(Color(0xFF4B2CB0))
                        } else {
                            Modifier
                                .background(Couleurs.carte)
                                .border(1.dp, Couleurs.degrade, forme)
                        },
                    )
                    .padding(horizontal = 14.dp, vertical = 10.dp),
        ) {
            Text(
                text = if (texte.isEmpty() && enCours) AnnotatedString("…") else markdownSimple(texte),
                style = MaterialTheme.typography.bodyLarge,
                color = Color.White,
            )
            if (!deMoi && !enCours) actions()
        }
    }
}

/**
 * Mise en forme légère du texte produit par l'IA : **gras**, *italique*, listes à puces et titres.
 */
fun markdownSimple(texte: String): AnnotatedString =
    buildAnnotatedString {
        val lignes = texte.replace("\r", "").split('\n')
        lignes.forEachIndexed { index, brute ->
            var ligne = brute
            val titre = Regex("^#{1,6}\\s+").find(ligne)
            if (titre != null) {
                withStyle(SpanStyle(fontWeight = FontWeight.Bold)) { ajouterEnLigne(ligne.substring(titre.range.last + 1)) }
            } else {
                val puce = Regex("^\\s*[-*+]\\s+").find(ligne)
                if (puce != null) {
                    append("• ")
                    ligne = ligne.substring(puce.range.last + 1)
                }
                ajouterEnLigne(ligne)
            }
            if (index < lignes.lastIndex) append('\n')
        }
    }

private fun AnnotatedString.Builder.ajouterEnLigne(texte: String) {
    var i = 0
    while (i < texte.length) {
        when {
            texte.startsWith("**", i) -> {
                val fin = texte.indexOf("**", i + 2)
                if (fin > i + 2) {
                    withStyle(SpanStyle(fontWeight = FontWeight.Bold)) { append(texte.substring(i + 2, fin)) }
                    i = fin + 2
                } else {
                    append("**")
                    i += 2
                }
            }
            texte[i] == '*' && i + 1 < texte.length && texte[i + 1] != ' ' -> {
                val fin = texte.indexOf('*', i + 1)
                if (fin > i + 1) {
                    withStyle(SpanStyle(fontStyle = FontStyle.Italic)) { append(texte.substring(i + 1, fin)) }
                    i = fin + 1
                } else {
                    append('*')
                    i++
                }
            }
            texte[i] == '`' -> i++
            else -> {
                append(texte[i])
                i++
            }
        }
    }
}
