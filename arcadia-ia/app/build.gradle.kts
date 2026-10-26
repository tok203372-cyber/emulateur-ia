import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "app.arcadia.ia"
    compileSdk = 36

    defaultConfig {
        applicationId = "app.arcadia.ia"
        minSdk = 31
        targetSdk = 35
        versionCode = 1
        versionName = "1.0.0"
        // Les modèles d'IA ne tournent bien que sur les téléphones 64 bits récents.
        ndk {
            abiFilters += "arm64-v8a"
        }
    }

    signingConfigs {
        create("arcadia") {
            // Même clé qu'Arcadia : les deux applications se reconnaissent et se mettent à jour.
            storeFile = file("../../arcadia.jks")
            storeType = "pkcs12"
            storePassword = "arcadia2026"
            keyAlias = "arcadia"
            keyPassword = "arcadia2026"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("arcadia")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
    }

    packaging {
        jniLibs {
            pickFirsts.add("**/liblitertlm_jni.so")
        }
    }

    lint {
        checkReleaseBuilds = false
        abortOnError = false
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
        // La bibliothèque LiteRT-LM est compilée avec une version plus récente de Kotlin.
        freeCompilerArgs.addAll("-Xskip-metadata-version-check")
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.7")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation(platform("androidx.compose:compose-bom:2026.02.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended:1.7.8")

    // Moteur de modèles de langage sur le téléphone (Google AI Edge).
    implementation("com.google.ai.edge.litertlm:litertlm-android:0.18.0")
}
