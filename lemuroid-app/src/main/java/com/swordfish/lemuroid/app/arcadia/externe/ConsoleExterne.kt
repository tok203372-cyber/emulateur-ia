package com.swordfish.lemuroid.app.arcadia.externe

import android.content.Intent

/**
 * Comment un émulateur externe reçoit le jeu à lancer.
 *
 * Les définitions viennent du code source des émulateurs quand il est public (Dolphin,
 * ARMSX2) et sinon des définitions utilisées par les lanceurs connus (Daijishō).
 */
enum class ModeTransmission {
    /** Le jeu est passé dans les données de l'intent (`intent.data`). */
    DONNEES,

    /** Le jeu est passé sous forme de texte dans un extra de l'intent. */
    EXTRA,

    /** Les deux à la fois (les versions récentes lisent les données, les anciennes l'extra). */
    DONNEES_ET_EXTRA,
}

data class EmulateurExterne(
    val nom: String,
    val paquet: String,
    val activite: String,
    val action: String = Intent.ACTION_VIEW,
    val mode: ModeTransmission = ModeTransmission.DONNEES,
    val cleExtra: String? = null,
    /** Page officielle où l'installer (null si aucune source officielle sûre n'est connue). */
    val lienInstallation: String? = null,
)

/** Consoles jouées grâce à un émulateur installé à côté d'Arcadia. */
enum class ConsoleExterne(
    val id: String,
    val nom: String,
    val nomCourt: String,
    val couleur: Long,
    val extensions: Set<String>,
    val emulateurs: List<EmulateurExterne>,
    /** Conseil affiché sous la console (formats recommandés, réglages…). */
    val conseil: String,
) {
    GAMECUBE_WII(
        id = "gc_wii",
        nom = "GameCube / Wii",
        nomCourt = "GC/Wii",
        couleur = 0xFF6A5ACD,
        extensions = setOf("iso", "gcm", "ciso", "gcz", "rvz", "wia", "wbfs", "wad", "dol", "elf", "tgc"),
        emulateurs =
            listOf(
                EmulateurExterne(
                    nom = "Dolphin",
                    paquet = "org.dolphinemu.dolphinemu",
                    activite = "org.dolphinemu.dolphinemu.ui.main.MainActivity",
                    action = Intent.ACTION_MAIN,
                    mode = ModeTransmission.DONNEES_ET_EXTRA,
                    cleExtra = "AutoStartFile",
                    lienInstallation = "https://play.google.com/store/apps/details?id=org.dolphinemu.dolphinemu",
                ),
                EmulateurExterne(
                    nom = "Dolphin MMJR",
                    paquet = "org.dolphinemu.mmjr",
                    activite = "org.dolphinemu.dolphinemu.ui.main.MainActivity",
                    mode = ModeTransmission.DONNEES_ET_EXTRA,
                    cleExtra = "AutoStartFile",
                ),
                EmulateurExterne(
                    nom = "Dolphin MMJR3",
                    paquet = "org.dolphinemu.mmjr3",
                    activite = "org.dolphinemu.dolphinemu.ui.main.MainActivity",
                    mode = ModeTransmission.DONNEES_ET_EXTRA,
                    cleExtra = "AutoStartFile",
                ),
            ),
        conseil =
            "Formats conseillés : .rvz ou .iso. La première fois, ouvrez Dolphin une fois pour " +
                "régler la manette (Manettes GameCube → Port 1 → Manette standard → Par défaut).",
    ),
    PS2(
        id = "ps2",
        nom = "PlayStation 2",
        nomCourt = "PS2",
        couleur = 0xFF2B59C3,
        extensions = setOf("iso", "chd", "cso", "zso", "bin", "gz", "elf", "img", "mdf"),
        emulateurs =
            listOf(
                EmulateurExterne(
                    nom = "ARMSX2",
                    paquet = "com.armsx2",
                    activite = "com.armsx2.Main",
                    lienInstallation = "https://github.com/ARMSX2/ARMSX2/releases",
                ),
                EmulateurExterne(
                    nom = "ARMSX2 (Play Store)",
                    paquet = "come.nanodata.armsx2",
                    activite = "com.armsx2.Main",
                    lienInstallation = "https://play.google.com/store/apps/details?id=come.nanodata.armsx2",
                ),
                EmulateurExterne(
                    nom = "NetherSX2",
                    paquet = "xyz.aethersx2.android",
                    activite = "xyz.aethersx2.android.EmulationActivity",
                    action = Intent.ACTION_MAIN,
                    mode = ModeTransmission.EXTRA,
                    cleExtra = "bootPath",
                ),
                EmulateurExterne(
                    nom = "NetherSX2 Turnip",
                    paquet = "xyz.aethersx2.tturnip",
                    activite = "xyz.aethersx2.android.EmulationActivity",
                    action = Intent.ACTION_MAIN,
                    mode = ModeTransmission.EXTRA,
                    cleExtra = "bootPath",
                ),
            ),
        conseil = "Formats conseillés : .chd ou .iso (un seul fichier par jeu). Le BIOS PS2 vient de votre console.",
    ),
    DREAMCAST(
        id = "dc",
        nom = "Dreamcast",
        nomCourt = "DC",
        couleur = 0xFFF07B26,
        extensions = setOf("chd", "cdi", "gdi", "cue", "zip"),
        emulateurs =
            listOf(
                EmulateurExterne(
                    nom = "Flycast",
                    paquet = "com.flycast.emulator",
                    activite = "com.flycast.emulator.MainActivity",
                    lienInstallation = "https://play.google.com/store/apps/details?id=com.flycast.emulator",
                ),
                EmulateurExterne(
                    nom = "Redream",
                    paquet = "io.recompiled.redream",
                    activite = "io.recompiled.redream.MainActivity",
                    lienInstallation = "https://play.google.com/store/apps/details?id=io.recompiled.redream",
                ),
            ),
        conseil = "Formats conseillés : .chd ou .cdi (un seul fichier par jeu).",
    ),
    SATURN(
        id = "saturn",
        nom = "Saturn",
        nomCourt = "Saturn",
        couleur = 0xFF4A4F5C,
        extensions = setOf("chd", "cue", "iso", "mds", "ccd"),
        emulateurs =
            listOf(
                EmulateurExterne(
                    nom = "Yaba Sanshiro 2 Pro",
                    paquet = "org.devmiyax.yabasanshioro2.pro",
                    activite = "org.uoyabause.android.Yabause",
                    action = Intent.ACTION_MAIN,
                    mode = ModeTransmission.EXTRA,
                    cleExtra = "org.uoyabause.android.FileNameUri",
                ),
                EmulateurExterne(
                    nom = "Yaba Sanshiro 2",
                    paquet = "org.devmiyax.yabasanshioro2",
                    activite = "org.uoyabause.android.Yabause",
                    mode = ModeTransmission.EXTRA,
                    cleExtra = "org.uoyabause.android.FileNameUri",
                    lienInstallation = "https://play.google.com/store/apps/details?id=org.devmiyax.yabasanshioro2",
                ),
            ),
        conseil = "Format conseillé : .chd (un seul fichier par jeu).",
    ),
    TROIS_DS(
        id = "3ds_azahar",
        nom = "3DS (Azahar)",
        nomCourt = "3DS",
        couleur = 0xFFD12C2C,
        extensions = setOf("3ds", "cci", "cxi", "3dsx", "app", "zcci", "zcxi", "z3dsx"),
        emulateurs =
            listOf(
                EmulateurExterne(
                    nom = "Azahar",
                    paquet = "org.azahar_emu.azahar",
                    activite = "org.citra.citra_emu.activities.EmulationActivity",
                    lienInstallation = "https://play.google.com/store/apps/details?id=org.azahar_emu.azahar",
                ),
                EmulateurExterne(
                    nom = "Azahar (GitHub)",
                    paquet = "io.github.lime3ds.android",
                    activite = "org.citra.citra_emu.activities.EmulationActivity",
                ),
            ),
        conseil =
            "Arcadia sait déjà lancer la 3DS tout seul. Cette entrée sert si vous préférez " +
                "l'émulateur Azahar (souvent plus rapide).",
    ),
    WII_U(
        id = "wiiu",
        nom = "Wii U",
        nomCourt = "Wii U",
        couleur = 0xFF1BA1E2,
        extensions = setOf("wud", "wux", "wua", "rpx", "wuhb"),
        emulateurs =
            listOf(
                EmulateurExterne(
                    nom = "Cemu",
                    paquet = "info.cemu.cemu",
                    activite = "info.cemu.cemu.emulation.EmulationActivity",
                ),
            ),
        conseil = "Format conseillé : .wua. Les clés et le microprogramme viennent de votre console.",
    ),
    SWITCH(
        id = "switch",
        nom = "Switch",
        nomCourt = "Switch",
        couleur = 0xFFE60012,
        extensions = setOf("nsp", "xci", "nro"),
        emulateurs =
            listOf(
                EmulateurExterne(
                    nom = "Eden",
                    paquet = "dev.eden.eden_emulator",
                    activite = "org.yuzu.yuzu_emu.activities.EmulationActivity",
                    action = "android.nfc.action.TECH_DISCOVERED",
                ),
                EmulateurExterne(
                    nom = "Citron",
                    paquet = "org.citron.citron_emu",
                    activite = "org.citron.citron_emu.activities.EmulationActivity",
                    action = "android.nfc.action.TECH_DISCOVERED",
                ),
                EmulateurExterne(
                    nom = "Sudachi",
                    paquet = "org.sudachi.sudachi_emu",
                    activite = "org.sudachi.sudachi_emu.activities.EmulationActivity",
                    action = "android.nfc.action.TECH_DISCOVERED",
                ),
                EmulateurExterne(
                    nom = "Kenji-NX",
                    paquet = "org.kenjinx.android",
                    activite = "org.kenjinx.android.MainActivity",
                    action = "org.kenjinx.android.LAUNCH_GAME",
                    mode = ModeTransmission.EXTRA,
                    cleExtra = "bootPath",
                ),
            ),
        conseil = "Les clés (prod.keys) et le microprogramme viennent de votre console.",
    ),
    PS3(
        id = "ps3",
        nom = "PlayStation 3",
        nomCourt = "PS3",
        couleur = 0xFF111111,
        extensions = setOf("iso"),
        emulateurs =
            listOf(
                EmulateurExterne(
                    nom = "aPS3e",
                    paquet = "aenu.aps3e",
                    activite = "aenu.aps3e.EmulatorActivity",
                    action = "aenu.intent.action.APS3E",
                    mode = ModeTransmission.EXTRA,
                    cleExtra = "iso_uri",
                ),
            ),
        conseil = "Expérimental sur téléphone. Le microprogramme PS3 s'installe dans l'émulateur.",
    ),
    ;

    companion object {
        fun parId(id: String?): ConsoleExterne? = values().firstOrNull { it.id == id }

        /** Tous les paquets connus (pour savoir lesquels sont installés). */
        fun tousLesPaquets(): List<String> = values().flatMap { c -> c.emulateurs.map { it.paquet } }.distinct()
    }
}
