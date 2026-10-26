package com.swordfish.lemuroid.app.arcadia.ia

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.widget.Toast
import androidx.core.content.FileProvider
import java.io.File

/**
 * Lien avec l'application « Arcadia IA », qui fait tourner le modèle de langage sur le téléphone
 * (assistant de jeu, recommandations, discussion).
 */
object CompagnonIA {
    const val PAQUET = "app.arcadia.ia"

    const val ACTION_ASSISTANT = "app.arcadia.ia.ASSISTANT"
    const val ACTION_RECOMMANDER = "app.arcadia.ia.RECOMMANDER"
    const val ACTION_ACCUEIL = "app.arcadia.ia.ACCUEIL"

    const val EXTRA_TITRE_JEU = "titre_jeu"
    const val EXTRA_CONSOLE = "console"
    const val EXTRA_TEXTE_ECRAN = "texte_ecran"
    const val EXTRA_QUESTION = "question"
    const val EXTRA_JEUX_CLES = "jeux_cles"
    const val EXTRA_JEUX_TITRES = "jeux_titres"
    const val EXTRA_JEUX_CONSOLES = "jeux_consoles"
    const val RESULTAT_CLE_JEU = "cle_jeu"

    const val DEMANDE_RECOMMANDATION = 4201

    fun estInstalle(context: Context): Boolean =
        try {
            if (Build.VERSION.SDK_INT >= 33) {
                context.packageManager.getPackageInfo(PAQUET, PackageManager.PackageInfoFlags.of(0))
            } else {
                @Suppress("DEPRECATION")
                context.packageManager.getPackageInfo(PAQUET, 0)
            }
            true
        } catch (e: PackageManager.NameNotFoundException) {
            false
        }

    private fun absent(context: Context) {
        Toast.makeText(
            context,
            "Installez l'application « Arcadia IA » pour utiliser l'assistant (voir le guide).",
            Toast.LENGTH_LONG,
        ).show()
    }

    fun ouvrirAccueil(activity: Activity) {
        val intent = Intent(ACTION_ACCUEIL).setPackage(PAQUET)
        try {
            activity.startActivity(intent)
        } catch (e: ActivityNotFoundException) {
            absent(activity)
        }
    }

    fun ouvrirAssistant(
        activity: Activity,
        titreJeu: String?,
        console: String?,
        texteEcran: String? = null,
        question: String? = null,
        image: File? = null,
    ) {
        val intent =
            Intent(ACTION_ASSISTANT).apply {
                setPackage(PAQUET)
                titreJeu?.let { putExtra(EXTRA_TITRE_JEU, it) }
                console?.let { putExtra(EXTRA_CONSOLE, it) }
                texteEcran?.let { putExtra(EXTRA_TEXTE_ECRAN, it.take(3000)) }
                question?.let { putExtra(EXTRA_QUESTION, it) }
                if (image != null) {
                    val uri = urifichier(activity, image)
                    if (uri != null) {
                        setDataAndType(uri, "image/jpeg")
                        clipData = ClipData.newRawUri("ecran", uri)
                        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    }
                }
            }
        try {
            activity.startActivity(intent)
        } catch (e: ActivityNotFoundException) {
            absent(activity)
        }
    }

    data class JeuPropose(val cle: String, val titre: String, val console: String)

    fun demanderRecommandation(
        activity: Activity,
        jeux: List<JeuPropose>,
    ) {
        val choisis = jeux.take(300)
        val intent =
            Intent(ACTION_RECOMMANDER).apply {
                setPackage(PAQUET)
                putStringArrayListExtra(EXTRA_JEUX_CLES, ArrayList(choisis.map { it.cle }))
                putStringArrayListExtra(EXTRA_JEUX_TITRES, ArrayList(choisis.map { it.titre }))
                putStringArrayListExtra(EXTRA_JEUX_CONSOLES, ArrayList(choisis.map { it.console }))
            }
        try {
            @Suppress("DEPRECATION")
            activity.startActivityForResult(intent, DEMANDE_RECOMMANDATION)
        } catch (e: ActivityNotFoundException) {
            absent(activity)
        }
    }

    private fun urifichier(
        context: Context,
        fichier: File,
    ): Uri? =
        runCatching {
            FileProvider.getUriForFile(context, "${context.packageName}.arcadia.fichiers", fichier)
        }.getOrNull()
}
