package com.swordfish.lemuroid.app.arcadia.externe

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.ClipData
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.widget.Toast
import timber.log.Timber

/** Lance un jeu externe dans le bon émulateur, en lui donnant le droit de lire le fichier. */
object LanceurExterne {
    fun estInstalle(
        context: Context,
        paquet: String,
    ): Boolean =
        try {
            if (Build.VERSION.SDK_INT >= 33) {
                context.packageManager.getPackageInfo(paquet, PackageManager.PackageInfoFlags.of(0))
            } else {
                @Suppress("DEPRECATION")
                context.packageManager.getPackageInfo(paquet, 0)
            }
            true
        } catch (e: PackageManager.NameNotFoundException) {
            false
        }

    fun emulateursInstalles(
        context: Context,
        console: ConsoleExterne,
    ): List<EmulateurExterne> = console.emulateurs.filter { estInstalle(context, it.paquet) }

    /** L'émulateur choisi par l'utilisateur s'il est installé, sinon le premier installé. */
    fun emulateurPourConsole(
        context: Context,
        console: ConsoleExterne,
    ): EmulateurExterne? {
        val installes = emulateursInstalles(context, console)
        val prefere = BibliothequeExterne.obtenir(context).emulateurPrefere(console)
        return installes.firstOrNull { it.paquet == prefere } ?: installes.firstOrNull()
    }

    sealed class Resultat {
        data class Lance(val emulateur: EmulateurExterne) : Resultat()

        data class EmulateurManquant(val console: ConsoleExterne) : Resultat()

        data class Erreur(val message: String) : Resultat()
    }

    fun construireIntent(
        emulateur: EmulateurExterne,
        uri: Uri,
    ): Intent =
        Intent(emulateur.action).apply {
            component = ComponentName(emulateur.paquet, emulateur.activite)
            when (emulateur.mode) {
                ModeTransmission.DONNEES -> data = uri
                ModeTransmission.EXTRA -> putExtra(emulateur.cleExtra, uri.toString())
                ModeTransmission.DONNEES_ET_EXTRA -> {
                    data = uri
                    putExtra(emulateur.cleExtra, uri.toString())
                }
            }
            // Le ClipData transporte l'autorisation de lecture même quand le jeu passe par un extra.
            clipData = ClipData.newRawUri("jeu", uri)
            addFlags(
                Intent.FLAG_GRANT_READ_URI_PERMISSION or
                    Intent.FLAG_ACTIVITY_NEW_TASK or
                    Intent.FLAG_ACTIVITY_CLEAR_TASK,
            )
        }

    fun lancer(
        activity: Activity,
        jeu: JeuExterne,
    ): Resultat {
        val console = jeu.console ?: return Resultat.Erreur("Console inconnue")
        val emulateur =
            emulateurPourConsole(activity, console)
                ?: return Resultat.EmulateurManquant(console)

        val intent = construireIntent(emulateur, Uri.parse(jeu.uri))
        return try {
            activity.startActivity(intent)
            Resultat.Lance(emulateur)
        } catch (e: ActivityNotFoundException) {
            Timber.e(e, "Arcadia : ${emulateur.nom} introuvable")
            ouvrirAvec(activity, jeu)
        } catch (e: SecurityException) {
            Timber.e(e, "Arcadia : ${emulateur.nom} refuse le lancement direct")
            ouvrirAvec(activity, jeu)
        }
    }

    /** Solution de secours : Android propose les applications capables d'ouvrir le fichier. */
    fun ouvrirAvec(
        activity: Activity,
        jeu: JeuExterne,
    ): Resultat {
        val uri = Uri.parse(jeu.uri)
        val intent =
            Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(uri, "application/octet-stream")
                clipData = ClipData.newRawUri("jeu", uri)
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
            }
        return try {
            activity.startActivity(Intent.createChooser(intent, jeu.titre))
            Resultat.Lance(EmulateurExterne("?", "", ""))
        } catch (e: ActivityNotFoundException) {
            Resultat.Erreur("Aucune application ne peut ouvrir ce jeu")
        }
    }

    fun ouvrirPageInstallation(
        context: Context,
        emulateur: EmulateurExterne,
    ) {
        val lien = emulateur.lienInstallation ?: return
        try {
            context.startActivity(
                Intent(Intent.ACTION_VIEW, Uri.parse(lien)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            )
        } catch (e: ActivityNotFoundException) {
            Toast.makeText(context, lien, Toast.LENGTH_LONG).show()
        }
    }

    /** Message clair quand le jeu ne peut pas démarrer. */
    fun expliquer(
        context: Context,
        resultat: Resultat,
    ) {
        val message =
            when (resultat) {
                is Resultat.Lance -> return
                is Resultat.EmulateurManquant -> {
                    val noms = resultat.console.emulateurs.joinToString(" ou ") { it.nom }
                    "Pour jouer à la ${resultat.console.nom}, installez d'abord $noms (onglet « Plus »)."
                }
                is Resultat.Erreur -> resultat.message
            }
        Toast.makeText(context, message, Toast.LENGTH_LONG).show()
    }
}
