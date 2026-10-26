package app.arcadia.ia

import android.content.Context
import java.io.File

/** Un modèle de langage que l'application sait utiliser. */
data class ModeleIA(
    val id: String,
    val nom: String,
    val description: String,
    val fichier: String,
    /** Adresse de téléchargement direct, ou null si un compte Hugging Face est nécessaire. */
    val url: String?,
    /** Page à ouvrir pour un téléchargement manuel. */
    val page: String?,
    val taille: Long,
    val vision: Boolean,
    val temperature: Double,
    val topK: Int,
    val topP: Double,
)

object Modeles {
    private const val HF = "https://huggingface.co"

    val catalogue =
        listOf(
            ModeleIA(
                id = "qwen25_15b",
                nom = "Qwen 2.5 — rapide",
                description = "Léger (1,6 Go) et rapide. Très bien pour les questions courtes et les recommandations.",
                fichier = "Qwen2.5-1.5B-Instruct_multi-prefill-seq_q8_ekv4096.litertlm",
                url =
                    "$HF/litert-community/Qwen2.5-1.5B-Instruct/resolve/19edb84c69a0212f29a6ef17ba0d6f278b6a1614/" +
                        "Qwen2.5-1.5B-Instruct_multi-prefill-seq_q8_ekv4096.litertlm?download=true",
                page = "$HF/litert-community/Qwen2.5-1.5B-Instruct",
                taille = 1_597_931_520L,
                vision = false,
                temperature = 0.7,
                topK = 20,
                topP = 0.8,
            ),
            ModeleIA(
                id = "phi4_mini",
                nom = "Phi-4 mini — plus malin",
                description = "Plus intelligent (3,9 Go), un peu plus lent. Conseillé sur un téléphone puissant comme le S25 Ultra.",
                fichier = "Phi-4-mini-instruct_multi-prefill-seq_q8_ekv4096.litertlm",
                url =
                    "$HF/litert-community/Phi-4-mini-instruct/resolve/054f4e2694a86f81a129a40596e08b8d74770a9d/" +
                        "Phi-4-mini-instruct_multi-prefill-seq_q8_ekv4096.litertlm?download=true",
                page = "$HF/litert-community/Phi-4-mini-instruct",
                taille = 3_910_090_752L,
                vision = false,
                temperature = 0.6,
                topK = 40,
                topP = 0.9,
            ),
            ModeleIA(
                id = "gemma3n_e4b",
                nom = "Gemma 3n E4B — voit l'écran",
                description =
                    "Comprend aussi les images : l'assistant regarde vraiment votre écran de jeu (4,9 Go). " +
                        "Google demande d'accepter sa licence : téléchargez-le sur Hugging Face puis importez-le.",
                fichier = "gemma-3n-E4B-it-int4.litertlm",
                url = null,
                page = "$HF/google/gemma-3n-E4B-it-litert-lm",
                taille = 4_919_541_760L,
                vision = true,
                temperature = 1.0,
                topK = 64,
                topP = 0.95,
            ),
            ModeleIA(
                id = "gemma3n_e2b",
                nom = "Gemma 3n E2B — voit l'écran (léger)",
                description =
                    "Version plus légère de Gemma 3n (3,7 Go), comprend aussi les images. " +
                        "À télécharger sur Hugging Face puis à importer.",
                fichier = "gemma-3n-E2B-it-int4.litertlm",
                url = null,
                page = "$HF/google/gemma-3n-E2B-it-litert-lm",
                taille = 3_655_827_456L,
                vision = true,
                temperature = 1.0,
                topK = 64,
                topP = 0.95,
            ),
        )

    fun dossier(context: Context): File =
        File(context.getExternalFilesDir(null) ?: context.filesDir, "modeles").apply { mkdirs() }

    fun fichier(
        context: Context,
        modele: ModeleIA,
    ): File = File(dossier(context), modele.fichier)

    fun estInstalle(
        context: Context,
        modele: ModeleIA,
    ): Boolean {
        val f = fichier(context, modele)
        return f.exists() && f.length() > modele.taille * 9 / 10
    }

    /** Modèles importés à la main (fichiers .litertlm qui ne sont pas dans le catalogue). */
    fun importes(context: Context): List<File> =
        dossier(context).listFiles()
            ?.filter { f -> f.isFile && f.name.endsWith(".litertlm") && catalogue.none { it.fichier == f.name } }
            ?.sortedBy { it.name }
            .orEmpty()

    /** Description d'un fichier importé (on devine s'il comprend les images d'après son nom). */
    fun depuisFichier(fichier: File): ModeleIA {
        val connu = catalogue.firstOrNull { it.fichier == fichier.name }
        if (connu != null) return connu
        val nomBas = fichier.name.lowercase()
        val vision = nomBas.contains("gemma-3n") || nomBas.contains("vision") || nomBas.contains("-vl")
        return ModeleIA(
            id = "importe:" + fichier.name,
            nom = fichier.nameWithoutExtension,
            description = "Modèle importé",
            fichier = fichier.name,
            url = null,
            page = null,
            taille = fichier.length(),
            vision = vision,
            temperature = 0.7,
            topK = 40,
            topP = 0.9,
        )
    }

    // ----- Réglages -----

    private fun prefs(context: Context) = context.getSharedPreferences("arcadia_ia", Context.MODE_PRIVATE)

    fun modeleChoisi(context: Context): ModeleIA? {
        val nom = prefs(context).getString("modele", null)
        val candidats =
            catalogue.filter { estInstalle(context, it) } + importes(context).map { depuisFichier(it) }
        return candidats.firstOrNull { it.fichier == nom } ?: candidats.firstOrNull()
    }

    fun choisir(
        context: Context,
        modele: ModeleIA,
    ) = prefs(context).edit().putString("modele", modele.fichier).apply()

    fun utiliserGpu(context: Context): Boolean = prefs(context).getBoolean("gpu", true)

    fun definirGpu(
        context: Context,
        actif: Boolean,
    ) = prefs(context).edit().putBoolean("gpu", actif).apply()

    fun lectureAuto(context: Context): Boolean = prefs(context).getBoolean("lecture_auto", false)

    fun definirLectureAuto(
        context: Context,
        actif: Boolean,
    ) = prefs(context).edit().putBoolean("lecture_auto", actif).apply()

    fun telechargementEnCours(context: Context): Map<String, Long> {
        val brut = prefs(context).getStringSet("telechargements", emptySet()).orEmpty()
        return brut.mapNotNull { ligne ->
            val (id, numero) = ligne.split('=', limit = 2).takeIf { it.size == 2 } ?: return@mapNotNull null
            numero.toLongOrNull()?.let { id to it }
        }.toMap()
    }

    fun noterTelechargement(
        context: Context,
        idModele: String,
        numero: Long?,
    ) {
        val actuels = telechargementEnCours(context).toMutableMap()
        if (numero == null) actuels.remove(idModele) else actuels[idModele] = numero
        prefs(context).edit().putStringSet("telechargements", actuels.map { "${it.key}=${it.value}" }.toSet()).apply()
    }
}
