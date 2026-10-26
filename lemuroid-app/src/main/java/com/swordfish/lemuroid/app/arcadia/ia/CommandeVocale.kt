package com.swordfish.lemuroid.app.arcadia.ia

import com.swordfish.lemuroid.app.arcadia.ui.normaliser

/** Ce que l'utilisateur demande à voix haute pendant une partie. */
sealed class Commande {
    data class Sauvegarder(val emplacement: Int?) : Commande()

    data class Charger(val emplacement: Int?) : Commande()

    object Quitter : Commande()

    object Recommencer : Commande()

    object AvanceRapide : Commande()

    object CouperSon : Commande()

    object RemettreSon : Commande()

    data class Traduction(val active: Boolean) : Commande()

    object LireEcran : Commande()

    data class ChangerDeJeu(val nom: String) : Commande()

    data class Question(val texte: String) : Commande()

    object Inconnue : Commande()
}

/**
 * Comprend les phrases courantes en français (et quelques-unes en anglais) sans Internet.
 * Les phrases qui ne sont pas des ordres deviennent des questions pour l'assistant IA.
 */
object CommandeVocale {
    val exemples =
        listOf(
            "« Sauvegarde » ou « sauvegarde dans l'emplacement 2 »",
            "« Charge la sauvegarde »",
            "« Traduis l'écran » / « arrête la traduction »",
            "« Lis l'écran »",
            "« Change de jeu pour Mario Kart »",
            "« Avance rapide »",
            "« Coupe le son » / « remets le son »",
            "« Recommence » / « quitte le jeu »",
            "« Comment je bats ce boss ? » (question à l'assistant)",
        )

    private val nombres =
        mapOf(
            "un" to 1, "une" to 1, "premier" to 1, "1" to 1, "one" to 1,
            "deux" to 2, "deuxieme" to 2, "second" to 2, "2" to 2, "two" to 2,
            "trois" to 3, "troisieme" to 3, "3" to 3, "three" to 3,
            "quatre" to 4, "quatrieme" to 4, "4" to 4, "four" to 4,
        )

    fun interpreter(phrase: String): Commande {
        val t = normaliser(phrase)
        if (t.isEmpty()) return Commande.Inconnue
        val mots = t.split(' ')

        fun contient(vararg expressions: String) = expressions.any { expr -> t.contains(expr) }

        fun commencePar(vararg expressions: String) = expressions.any { expr -> t.startsWith(expr) }

        val emplacement = mots.firstNotNullOfOrNull { nombres[it] }

        // Changer de jeu : « change de jeu pour X », « lance X », « joue a X », « play X ».
        val changement =
            Regex(
                "^(?:change(?:r|z)? (?:de )?jeu(?: pour| a| avec)?|lance(?:r|z)?(?: le jeu)?|" +
                    "joue(?:r|z)? a|je veux jouer a|mets(?: le jeu)?|play|launch|switch to) (.+)$",
            ).find(t)
        if (changement != null) {
            val nom = changement.groupValues[1].removePrefix("le jeu ").trim()
            if (nom.isNotEmpty()) return Commande.ChangerDeJeu(nom)
        }

        return when {
            contient("arrete la traduction", "desactive la traduction", "stop traduction", "coupe la traduction", "stop translat") ->
                Commande.Traduction(false)
            contient("tradui", "traduction", "translate") -> Commande.Traduction(true)
            contient("lis l ecran", "lire l ecran", "lis le texte", "lis moi", "lecture", "read") -> Commande.LireEcran
            contient("sauvegard", "enregistr", "save") -> Commande.Sauvegarder(emplacement)
            contient("charge", "recharge", "load", "reprend") -> Commande.Charger(emplacement)
            contient("avance rapide", "accelere", "plus vite", "turbo", "fast forward") -> Commande.AvanceRapide
            contient("remets le son", "remet le son", "active le son", "unmute", "son on") -> Commande.RemettreSon
            contient("coupe le son", "silence", "muet", "mute", "sans le son") -> Commande.CouperSon
            commencePar("recommence", "redemarre", "reinitialise", "restart", "reset") -> Commande.Recommencer
            contient("quitte", "quitter", "ferme le jeu", "arrete le jeu", "sortir du jeu", "quit", "exit") -> Commande.Quitter
            mots.size >= 3 || t.endsWith("?") -> Commande.Question(phrase.trim())
            else -> Commande.Inconnue
        }
    }
}
