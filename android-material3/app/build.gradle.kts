import java.security.MessageDigest
import java.util.zip.ZipFile

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}
android {
    namespace = "org.isomorphisms.fastchat.material3"
    compileSdk = 34
    defaultConfig {
        applicationId = "org.isomorphisms.fastchat.material3"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0-m3"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk { abiFilters += "armeabi-v7a" }
    }
    signingConfigs {
        create("publicDevelopment") {
            storeFile = rootProject.file("signing/fastchat-material3-dev.jks")
            storePassword = "fastchat-development"
            keyAlias = "fastchat-material3-dev"
            keyPassword = "fastchat-development"
        }
    }
    buildTypes {
        debug { signingConfig = signingConfigs.getByName("publicDevelopment") }
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            signingConfig = signingConfigs.getByName("publicDevelopment")
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    buildFeatures { compose = true }
}
dependencies {
    implementation(project(":core"))
    val compose = platform("androidx.compose:compose-bom:2024.09.02")
    implementation(compose)
    androidTestImplementation(compose)
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.6")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.foundation:foundation")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.8.1")
    androidTestImplementation("androidx.compose.ui:ui-test-junit4")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test:runner:1.6.2")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
}

tasks.register("verifyM3Apk") {
    dependsOn("assembleRelease")
    doLast {
        val apk = layout.buildDirectory.file("outputs/apk/release/app-release.apk").get().asFile
        fun runTool(vararg args: String): String {
            val process = ProcessBuilder(*args).redirectErrorStream(true).start()
            val output = process.inputStream.bufferedReader().readText()
            check(process.waitFor() == 0) { output }
            return output
        }
        val tools = android.sdkDirectory.resolve("build-tools/34.0.0")
        val signer = runTool(tools.resolve("apksigner").path, "verify", "--verbose", "--print-certs", apk.path)
        val expected = "aa9151e3922fa4795987c3655bc7aa4712620f1d94c24a2075acd7b29d8ac86e"
        val certificates = Regex("Signer #\\d+ certificate SHA-256 digest: ([a-f0-9]+)").findAll(signer).map { it.groupValues[1] }.toList()
        check(certificates == listOf(expected)) { "Unexpected development signer: $signer" }
        val badging = runTool(tools.resolve("aapt2").path, "dump", "badging", apk.path)
        check(badging.contains("package: name='org.isomorphisms.fastchat.material3' versionCode='1'")) { badging }
        check(badging.contains("sdkVersion:'26'") && badging.contains("targetSdkVersion:'34'")) { badging }
        val nativeSizes = mutableListOf<String>()
        val nativeEntries = ZipFile(apk).use { zip ->
            zip.entries().asSequence().filter { it.name.startsWith("lib/") && it.name.endsWith(".so") }.map { entry ->
                val extracted = layout.buildDirectory.file("reports/native/${entry.name}").get().asFile
                extracted.parentFile.mkdirs()
                zip.getInputStream(entry).use { input -> extracted.outputStream().use { output -> input.copyTo(output) } }
                val header = runTool("readelf", "--file-header", extracted.path)
                check(Regex("Class:\\s+ELF32").containsMatchIn(header) && Regex("Machine:\\s+ARM\\s").containsMatchIn(header)) {
                    "Native payload does not match A1 ARM32: ${entry.name}\n$header"
                }
                val sections = runTool("readelf", "--sections", extracted.path)
                check(!Regex("\\.(debug_|zdebug_|symtab)").containsMatchIn(sections)) { "Unstripped native dependency: ${entry.name}" }
                val original = layout.buildDirectory.file("intermediates/merged_native_libs/release/mergeReleaseNativeLibs/out/${entry.name}").get().asFile
                nativeSizes += "${entry.name}\t${if (original.isFile) original.length().toString() else "UNKNOWN"}\t${entry.size}"
                entry.name
            }.toList()
        }
        check(nativeEntries.all { it.startsWith("lib/armeabi-v7a/") }) { "Unexpected native ABI: $nativeEntries" }
        val digest = MessageDigest.getInstance("SHA-256").digest(apk.readBytes()).joinToString("") { "%02x".format(it) }
        val source = runTool("git", "-C", rootProject.projectDir.parentFile.path, "rev-parse", "HEAD").trim()
        val dirty = runTool("git", "-C", rootProject.projectDir.parentFile.path, "status", "--porcelain").isNotBlank()
        val receipt = layout.buildDirectory.file("reports/m3-artifact.tsv").get().asFile
        receipt.parentFile.mkdirs()
        receipt.resolveSibling("m3-native-sizes.tsv").writeText("path\tpre_strip_bytes\tapk_native_bytes\n" + nativeSizes.joinToString("\n") + "\n")
        receipt.writeText("field\tvalue\nsource_commit\t$source\nsource_dirty\t$dirty\napk_bytes\t${apk.length()}\napk_sha256\t$digest\nsigner_sha256\t$expected\napplication_id\torg.isomorphisms.fastchat.material3\nversion_code\t1\nabi\tarmeabi-v7a\nnative_entries\t${nativeEntries.joinToString(",")}\nbuild\tPASS\npackage\tPASS\ninstallation\tNOT_RUN\nlaunch\tNOT_RUN\nphysical_miro_a1\tNOT_RUN\n")
        println(receipt.readText())
    }
}
