package com.swordfish.lemuroid.app.arcadia.externe

import android.content.Context
import android.net.Uri
import java.io.FileInputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.Charset

/**
 * Lit l'en-tête d'un disque GameCube ou Wii pour connaître son code (ex. GLME01), son vrai titre
 * et sa console. Fonctionne avec les formats courants : ISO/GCM, NKit, RVZ/WIA, WBFS, CISO, GCZ.
 * Rien n'est modifié : on lit seulement quelques centaines d'octets au début du fichier.
 */
object EnTeteDisque {
    enum class Plateforme { GAMECUBE, WII, INCONNUE }

    data class Infos(
        val code: String?,
        val titre: String?,
        val plateforme: Plateforme,
    )

    private const val MAGIE_WII = 0x5D1C9EA3
    private const val MAGIE_GC = 0xC2339F3D.toInt()

    fun lire(
        context: Context,
        uri: Uri,
        extension: String,
    ): Infos? =
        runCatching {
            context.contentResolver.openFileDescriptor(uri, "r")?.use { pfd ->
                FileInputStream(pfd.fileDescriptor).use { flux ->
                    val canal = flux.channel
                    fun lireA(position: Long, taille: Int): ByteArray? {
                        val tampon = ByteBuffer.allocate(taille)
                        var lu = 0
                        while (lu < taille) {
                            val n = canal.read(tampon, position + lu)
                            if (n <= 0) break
                            lu += n
                        }
                        return if (lu == taille) tampon.array() else null
                    }

                    val debut = lireA(0, 0x100) ?: return@use null
                    val magie = ByteBuffer.wrap(debut, 0, 4).order(ByteOrder.BIG_ENDIAN).int

                    when {
                        // RVZ / WIA : l'en-tête du disque est recopié à l'octet 0x58.
                        magie == 0x52565A01 || magie == 0x57494101 -> {
                            val typeDisque = ByteBuffer.wrap(debut, 0x48, 4).order(ByteOrder.BIG_ENDIAN).int
                            val plateforme =
                                when (typeDisque) {
                                    1 -> Plateforme.GAMECUBE
                                    2 -> Plateforme.WII
                                    else -> Plateforme.INCONNUE
                                }
                            enTete(debut, 0x58, plateforme)
                        }
                        // WBFS (Wii uniquement) : en-tête du disque au début du 2e secteur.
                        magie == 0x57424653 -> {
                            val tailleSecteur = 1L shl (debut[8].toInt() and 0xFF)
                            val bloc = lireA(tailleSecteur, 0x60) ?: return@use null
                            enTete(bloc, 0, Plateforme.WII)
                        }
                        // CISO : les données commencent à 0x8000.
                        magie == 0x4349534F -> {
                            val bloc = lireA(0x8000, 0x60) ?: return@use null
                            enTete(bloc, 0, plateformeBrute(bloc))
                        }
                        // GCZ (petit-boutiste) : le type est connu, le titre est compressé.
                        ByteBuffer.wrap(debut, 0, 4).order(ByteOrder.LITTLE_ENDIAN).int == 0xB10BC001.toInt() -> {
                            val sousType = ByteBuffer.wrap(debut, 4, 4).order(ByteOrder.LITTLE_ENDIAN).int
                            Infos(null, null, if (sousType == 1) Plateforme.WII else Plateforme.GAMECUBE)
                        }
                        extension == "wad" -> Infos(null, null, Plateforme.WII)
                        extension == "dol" || extension == "elf" -> Infos(null, null, Plateforme.INCONNUE)
                        // ISO / GCM / NKit : en-tête en clair au début.
                        else -> enTete(debut, 0, plateformeBrute(debut))
                    }
                }
            }
        }.getOrNull()

    private fun plateformeBrute(octets: ByteArray): Plateforme {
        if (octets.size < 0x20) return Plateforme.INCONNUE
        val wii = ByteBuffer.wrap(octets, 0x18, 4).order(ByteOrder.BIG_ENDIAN).int
        val gc = ByteBuffer.wrap(octets, 0x1C, 4).order(ByteOrder.BIG_ENDIAN).int
        return when {
            wii == MAGIE_WII -> Plateforme.WII
            gc == MAGIE_GC -> Plateforme.GAMECUBE
            else -> Plateforme.INCONNUE
        }
    }

    private fun enTete(
        octets: ByteArray,
        decalage: Int,
        plateforme: Plateforme,
    ): Infos? {
        if (octets.size < decalage + 0x60) return null
        val code = String(octets, decalage, 6, Charsets.US_ASCII)
        if (!code.all { it.isLetterOrDigit() }) return null
        val brut = octets.copyOfRange(decalage + 0x20, decalage + 0x60)
        val fin = brut.indexOfFirst { it.toInt() == 0 }.let { if (it < 0) brut.size else it }
        val utiles = brut.copyOf(fin)
        val titre =
            if (utiles.any { it < 0 }) {
                runCatching { String(utiles, Charset.forName("Shift_JIS")) }.getOrNull()
            } else {
                String(utiles, Charsets.US_ASCII)
            }
        return Infos(code, titre?.trim()?.takeIf { it.length >= 2 }, plateforme)
    }

    /**
     * Adresse de la jaquette sur GameTDB (la même source que Dolphin).
     * La lettre de région est le 4e caractère du code du jeu.
     */
    fun urlJaquette(code: String?): String? {
        if (code == null || code.length < 4) return null
        val region =
            when (code[3]) {
                'E', 'N' -> "US"
                'J' -> "JA"
                'K', 'Q', 'T' -> "KO"
                'D' -> "DE"
                'F' -> "FR"
                'I' -> "IT"
                'S' -> "ES"
                'H' -> "NL"
                'R' -> "RU"
                'U' -> "AU"
                else -> "EN"
            }
        return "https://art.gametdb.com/wii/cover/$region/$code.png"
    }
}
