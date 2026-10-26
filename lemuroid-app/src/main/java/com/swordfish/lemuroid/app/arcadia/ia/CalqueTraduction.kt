package com.swordfish.lemuroid.app.arcadia.ia

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Translate
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val fondBulle = Color(0xEB0F0B1E)
private val bordBulle = Color(0xFF4FE3D0)

/** Calque posé sur l'image du jeu : les textes traduits et les petits messages de l'IA. */
@Composable
fun CalqueTraduction(modifier: Modifier = Modifier) {
    val etat by TraductionEnDirect.etat.collectAsState()
    if (!etat.active && etat.bulles.isEmpty() && etat.message == null) return

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val largeur = maxWidth
        val hauteur = maxHeight

        etat.bulles.forEach { bulle ->
            val x = largeur * bulle.gauche
            val y = hauteur * bulle.haut
            val l = largeur * (bulle.droite - bulle.gauche)
            val h = hauteur * (bulle.bas - bulle.haut)
            Box(
                modifier =
                    Modifier
                        .offset(x = x, y = y)
                        .widthIn(min = l, max = maxOf(l, largeur * 0.62f))
                        .heightIn(min = h)
                        .background(fondBulle, RoundedCornerShape(6.dp))
                        .border(1.dp, bordBulle.copy(alpha = 0.55f), RoundedCornerShape(6.dp))
                        .padding(horizontal = 5.dp, vertical = 2.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                Text(
                    text = bulle.texte,
                    color = Color.White,
                    fontSize = tailleTexte(h, bulle.lignes),
                    lineHeight = tailleTexte(h, bulle.lignes) * 1.15f,
                    fontWeight = FontWeight.Medium,
                )
            }
        }

        if (etat.active) {
            Row(
                modifier =
                    Modifier
                        .align(Alignment.TopStart)
                        .padding(8.dp)
                        .background(Color(0x99000000), RoundedCornerShape(50))
                        .padding(horizontal = 8.dp, vertical = 3.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(Icons.Filled.Translate, contentDescription = null, tint = bordBulle, modifier = Modifier.size(14.dp))
                Text(
                    text = " IA",
                    color = Color.White,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold,
                )
            }
        }

        AnimatedVisibility(
            visible = etat.message != null,
            enter = fadeIn(),
            exit = fadeOut(),
            modifier =
                Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 16.dp),
        ) {
            Text(
                text = etat.message ?: "",
                color = Color.White,
                fontSize = 13.sp,
                modifier =
                    Modifier
                        .background(Color(0xCC1B1430), RoundedCornerShape(50))
                        .border(1.dp, bordBulle.copy(alpha = 0.6f), RoundedCornerShape(50))
                        .padding(horizontal = 14.dp, vertical = 6.dp),
            )
        }
    }
}

@Composable
private fun tailleTexte(
    hauteurBloc: Dp,
    lignes: Int,
): TextUnit {
    val densite = LocalDensity.current
    val parLigne = hauteurBloc / maxOf(1, lignes)
    val taille = with(densite) { (parLigne * 0.62f).toSp() }
    return when {
        taille.value < 9f -> 9.sp
        taille.value > 22f -> 22.sp
        else -> taille
    }
}
