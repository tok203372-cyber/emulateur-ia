package com.swordfish.lemuroid.app.arcadia.externe

import org.json.JSONObject
import java.io.Serializable

/** Un jeu joué avec un émulateur externe (GameCube, Wii, PS2…). */
data class JeuExterne(
    val consoleId: String,
    val uri: String,
    val nomFichier: String,
    val titre: String,
    /** Console précise quand on la connaît (ex. « GameCube » ou « Wii »), sinon le nom de la catégorie. */
    val etiquette: String,
    val code: String? = null,
    val jaquette: String? = null,
    val taille: Long = 0,
    val dernierePartie: Long? = null,
    val favori: Boolean = false,
) : Serializable {
    val console: ConsoleExterne?
        get() = ConsoleExterne.parId(consoleId)

    fun versJson(): JSONObject =
        JSONObject().apply {
            put("console", consoleId)
            put("uri", uri)
            put("fichier", nomFichier)
            put("titre", titre)
            put("etiquette", etiquette)
            code?.let { put("code", it) }
            jaquette?.let { put("jaquette", it) }
            put("taille", taille)
            dernierePartie?.let { put("derniere", it) }
            put("favori", favori)
        }

    companion object {
        private const val serialVersionUID = 1L

        fun depuisJson(o: JSONObject): JeuExterne? =
            runCatching {
                JeuExterne(
                    consoleId = o.getString("console"),
                    uri = o.getString("uri"),
                    nomFichier = o.optString("fichier"),
                    titre = o.getString("titre"),
                    etiquette = o.optString("etiquette"),
                    code = o.optString("code").takeIf { it.isNotEmpty() },
                    jaquette = o.optString("jaquette").takeIf { it.isNotEmpty() },
                    taille = o.optLong("taille", 0),
                    dernierePartie = if (o.has("derniere")) o.getLong("derniere") else null,
                    favori = o.optBoolean("favori", false),
                )
            }.getOrNull()

        /**
         * Transforme un nom de fichier en titre lisible :
         * « Luigi's Mansion (Europe) (En,Fr,De).rvz » devient « Luigi's Mansion ».
         */
        fun titreDepuisFichier(nomFichier: String): String {
            var t = nomFichier.substringBeforeLast('.')
            if (t.endsWith(".nkit", ignoreCase = true)) t = t.dropLast(5)
            t = t.replace(Regex("""\s*[\(\[][^\)\]]*[\)\]]"""), "")
            t = t.replace('_', ' ').replace(Regex("""\s+"""), " ").trim()
            return t.ifEmpty { nomFichier }
        }
    }
}
