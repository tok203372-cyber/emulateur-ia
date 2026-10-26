package app.arcadia.ia

/** Ce qu'Arcadia envoie à l'application (mêmes noms que dans Arcadia). */
object Contrat {
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
}

/** Les consignes données au modèle de langage. */
object Consignes {
    fun assistant(
        contexte: ContexteJeu?,
        vision: Boolean,
    ): String =
        buildString {
            appendLine(
                "Tu es Arcadia, un assistant de jeux vidéo qui fonctionne entièrement sur le téléphone " +
                    "de l'utilisateur, dans l'émulateur Arcadia.",
            )
            appendLine("Règles :")
            appendLine(
                "- Réponds toujours en français, de façon claire, chaleureuse et concise " +
                    "(5 phrases au plus, sauf si l'utilisateur demande des détails).",
            )
            appendLine("- Utilise de courtes listes à puces quand c'est utile.")
            appendLine(
                "- Si tu n'es pas sûr d'une information précise sur un jeu (emplacement exact, solution " +
                    "d'une énigme, code), dis-le franchement et propose des pistes générales au lieu d'inventer.",
            )
            appendLine("- L'utilisateur joue avec ses propres jeux : ne parle pas de télécharger des jeux.")
            if (contexte?.titre != null) {
                append("Jeu en cours : ${contexte.titre}")
                if (contexte.console != null) append(" (console : ${contexte.console})")
                appendLine(".")
            }
            val texte = contexte?.texteEcran
            if (!texte.isNullOrBlank()) {
                appendLine(
                    "Texte visible en ce moment sur l'écran du jeu (lu automatiquement, il peut contenir " +
                        "des erreurs ou être dans une autre langue) :",
                )
                appendLine("\"\"\"")
                appendLine(texte.take(1500))
                appendLine("\"\"\"")
            }
            if (vision && contexte?.image != null) {
                appendLine("Une capture de l'écran du jeu accompagne le premier message : appuie-toi dessus.")
            }
        }

    fun suggestions(contexte: ContexteJeu?): List<String> =
        if (contexte?.titre != null) {
            listOf(
                "Je suis bloqué, que dois-je faire ?",
                "Explique-moi ce qui est écrit à l'écran",
                "Donne-moi une astuce pour ce jeu",
                "Quels sont les contrôles de base ?",
                "Raconte-moi l'histoire du jeu",
            )
        } else {
            listOf(
                "Conseille-moi un jeu de course rétro",
                "C'est quoi un émulateur ?",
                "Quels sont les meilleurs jeux GameCube ?",
                "Donne-moi une astuce pour Mario Kart",
            )
        }

    fun recommandation(
        envie: String,
        jeux: List<Pair<String, String>>,
    ): String =
        buildString {
            appendLine("Voici la liste numérotée des jeux de l'utilisateur (titre — console) :")
            jeux.forEachIndexed { i, (titre, console) -> appendLine("${i + 1}. $titre — $console") }
            appendLine()
            appendLine("Envie de l'utilisateur : « $envie »")
            appendLine()
            appendLine(
                "Choisis les 3 jeux de la liste qui correspondent le mieux à cette envie. Réponds uniquement " +
                    "avec 3 lignes, au format exact : numéro | une phrase en français qui explique pourquoi.",
            )
            appendLine("Exemple :")
            appendLine("12 | Des courses rapides et faciles à prendre en main, parfaites à plusieurs.")
        }

    const val CONSIGNE_RECOMMANDATION =
        "Tu es Arcadia, un expert en jeux vidéo. Tu choisis des jeux uniquement dans la liste fournie et tu " +
            "réponds exactement au format demandé, en français."
}
