package com.swordfish.lemuroid.app.arcadia.ia

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Rect
import android.graphics.RectF
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.view.PixelCopy
import com.swordfish.libretrodroid.GLRetroView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import kotlin.coroutines.resume
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/** Une bulle de traduction posée sur l'image du jeu (coordonnées entre 0 et 1). */
data class Bulle(
    val gauche: Float,
    val haut: Float,
    val droite: Float,
    val bas: Float,
    val texte: String,
    val lignes: Int,
)

/** Photographie l'image du jeu (sans les boutons tactiles) pour la faire lire par l'IA. */
object CaptureJeu {
    /** @param zone partie de la vue occupée par le jeu, en fractions (0..1) de la vue entière. */
    data class Capture(val image: Bitmap, val zone: RectF)

    suspend fun capturer(
        vue: GLRetroView,
        maxCote: Int = 1280,
    ): Capture? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return null
        val largeur = vue.width
        val hauteur = vue.height
        if (largeur <= 0 || hauteur <= 0) return null

        val zone = RectF(vue.viewport)
        val source =
            Rect(
                (zone.left * largeur).roundToInt().coerceIn(0, largeur),
                (zone.top * hauteur).roundToInt().coerceIn(0, hauteur),
                (zone.right * largeur).roundToInt().coerceIn(0, largeur),
                (zone.bottom * hauteur).roundToInt().coerceIn(0, hauteur),
            )
        if (source.width() <= 1 || source.height() <= 1) return null

        val echelle = min(1f, maxCote.toFloat() / max(source.width(), source.height()))
        val image =
            Bitmap.createBitmap(
                max(1, (source.width() * echelle).roundToInt()),
                max(1, (source.height() * echelle).roundToInt()),
                Bitmap.Config.ARGB_8888,
            )
        val reussi: Boolean =
            suspendCancellableCoroutine<Boolean> { suite ->
                try {
                    PixelCopy.request(
                        vue,
                        source,
                        image,
                        { resultat -> if (suite.isActive) suite.resume(resultat == PixelCopy.SUCCESS) },
                        Handler(Looper.getMainLooper()),
                    )
                } catch (e: Exception) {
                    if (suite.isActive) suite.resume(false)
                }
            }
        if (!reussi) {
            image.recycle()
            return null
        }
        return Capture(image, zone)
    }

    /** Petite empreinte de l'image pour savoir si l'écran a changé depuis la dernière analyse. */
    fun empreinte(image: Bitmap): IntArray {
        val petite = Bitmap.createScaledBitmap(image, 16, 16, true)
        val pixels = IntArray(256)
        petite.getPixels(pixels, 0, 16, 0, 0, 16, 16)
        if (petite != image) petite.recycle()
        return IntArray(256) { i ->
            val c = pixels[i]
            (((c shr 16) and 0xFF) * 3 + ((c shr 8) and 0xFF) * 6 + (c and 0xFF)) / 10
        }
    }

    fun difference(
        a: IntArray,
        b: IntArray,
    ): Int {
        if (a.size != b.size) return Int.MAX_VALUE
        var total = 0
        for (i in a.indices) total += abs(a[i] - b[i])
        return total / a.size
    }

    fun versBulles(
        capture: Capture,
        blocs: List<LecteurEcran.BlocTraduit>,
        garderOriginaux: Boolean = false,
    ): List<Bulle> {
        val largeur = capture.image.width.toFloat()
        val hauteur = capture.image.height.toFloat()
        val z = capture.zone
        return blocs.mapNotNull { t ->
            val texte = t.traduction ?: if (garderOriginaux) t.bloc.texte else return@mapNotNull null
            val r = t.bloc.zone
            Bulle(
                gauche = z.left + r.left / largeur * z.width(),
                haut = z.top + r.top / hauteur * z.height(),
                droite = z.left + r.right / largeur * z.width(),
                bas = z.top + r.bottom / hauteur * z.height(),
                texte = texte,
                lignes = t.bloc.lignes,
            )
        }
    }

    /** Enregistre l'image en JPEG dans le cache (pour la montrer à l'assistant IA). */
    suspend fun enregistrer(
        context: Context,
        image: Bitmap,
        nom: String = "ecran.jpg",
    ): File =
        withContext(Dispatchers.IO) {
            val dossier = File(context.cacheDir, "arcadia").apply { mkdirs() }
            val fichier = File(dossier, nom)
            FileOutputStream(fichier).use { image.compress(Bitmap.CompressFormat.JPEG, 88, it) }
            fichier
        }
}
