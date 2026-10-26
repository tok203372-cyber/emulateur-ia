package com.swordfish.lemuroid.app.arcadia

import android.app.Activity
import android.content.Context
import android.content.Intent
import com.swordfish.lemuroid.app.arcadia.externe.BibliothequeExterne
import com.swordfish.lemuroid.app.arcadia.externe.JeuExterne
import com.swordfish.lemuroid.app.arcadia.externe.LanceurExterne
import com.swordfish.lemuroid.app.shared.game.GameProcessLock
import com.swordfish.lemuroid.lib.library.db.RetrogradeDatabase
import com.swordfish.lemuroid.lib.library.db.entity.Game
import kotlinx.coroutines.delay
import timber.log.Timber
import java.io.Serializable

/** Le jeu à lancer juste après celui qu'on quitte (« Changer de jeu »). */
sealed class ProchainJeu : Serializable {
    data class Interne(val idJeu: Int, val titre: String) : ProchainJeu()

    data class Externe(val jeu: JeuExterne) : ProchainJeu()

    companion object {
        private const val serialVersionUID = 1L

        const val EXTRA = "ARCADIA_PROCHAIN_JEU"

        fun depuis(intent: Intent?): ProchainJeu? =
            runCatching {
                @Suppress("DEPRECATION")
                intent?.getSerializableExtra(EXTRA) as? ProchainJeu
            }.getOrNull()
    }
}

/**
 * Enchaîne les jeux : attend que le jeu précédent ait bien fermé son processus (sauvegarde
 * comprise), puis lance le suivant avec le bon émulateur.
 */
object Enchainement {
    suspend fun attendreFinDuJeuPrecedent(context: Context) {
        val debut = System.currentTimeMillis()
        while (System.currentTimeMillis() - debut < 8000) {
            val occupe =
                runCatching { GameProcessLock.isHeldByAnotherProcess(context.applicationContext) }
                    .getOrDefault(false)
            if (!occupe) return
            delay(120)
        }
        Timber.w("Arcadia : le jeu précédent met longtemps à se fermer")
    }

    /**
     * @param lancerInterne lance un jeu d'Arcadia (avec les vérifications habituelles).
     */
    suspend fun lancer(
        activity: Activity,
        prochain: ProchainJeu,
        base: RetrogradeDatabase,
        lancerInterne: (Game) -> Unit,
    ) {
        attendreFinDuJeuPrecedent(activity)
        when (prochain) {
            is ProchainJeu.Interne -> {
                val jeu = base.gameDao().selectById(prochain.idJeu)
                if (jeu != null) {
                    lancerInterne(jeu)
                } else {
                    Timber.w("Arcadia : jeu ${prochain.idJeu} introuvable")
                }
            }
            is ProchainJeu.Externe -> {
                val resultat = LanceurExterne.lancer(activity, prochain.jeu)
                if (resultat is LanceurExterne.Resultat.Lance) {
                    BibliothequeExterne.obtenir(activity).noterPartie(prochain.jeu)
                } else {
                    LanceurExterne.expliquer(activity, resultat)
                }
            }
        }
    }
}
