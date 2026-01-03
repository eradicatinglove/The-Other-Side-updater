#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>
#include <minizip/unzip.h>
#include <switch/services/spsm.h>
#include <sys/stat.h>
#include <string>
#include <vector>

// Your GitHub API endpoints for latest releases
const std::string atmosApiUrl = "https://api.github.com/repos/eradicatinglove/Atmosphere/releases/latest";
const std::string hekateApiUrl = "https://api.github.com/repos/eradicatinglove/hekate/releases/latest";

// Direct links to your packs
const std::string atmosphereUrl = "https://github.com/eradicatinglove/Atmosphere/releases/download/v1.10.1/TheOtherSide_ams_1.10.1.zip";
const std::string hekateUrl      = "https://github.com/eradicatinglove/hekate/releases/download/v6.4.2/TheOtherSide_hekate_6.4.2.zip";

// App self-update URL
const std::string appUpdateUrl = "https://github.com/eradicatinglove/the-other-side-updater/releases/latest/download/the_other_side_updater.nro";

// Firmware packs - add more to have choices
const std::vector<std::string> firmwareUrls = {
    "https://mega.nz/file/VL5ClBoZ#KeP2NE-GTnLnmuME73tlvBo0-UgDEG3GkNxLw5BsUAQ",
    "https://mega.nz/file/FbZSUZpY#hvh1aCTM-m5ZyzDgk7BHQR204TrbwG8WjZPXOsMXNlU"
};

const std::vector<std::string> firmwareNames = {
    "Firmware (21.1.0)",
    "Firmware (20.5.0)"
};

static size_t write_data(void* ptr, size_t size, size_t nmemb, void* stream) {
    return fwrite(ptr, size, nmemb, (FILE*)stream);
}

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal > 0) {
        int percent = (int)((dlnow * 100LL) / dltotal);
        printf("\x1b[18;1HDownloading... %d%% (%ld / %ld bytes)", percent, (long)dlnow, (long)dltotal);
        consoleUpdate(NULL);
    }
    return 0;
}

bool downloadFile(const std::string& url, const std::string& path, bool showProgress = true) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "TheOtherSide-Updater/1.0");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, showProgress ? 0L : 1L);
    if (showProgress) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);
    return (res == CURLE_OK);
}

std::string fetchLatestVersion(const std::string& apiUrl) {
    std::string jsonPath = "/switch/temp.json";
    if (downloadFile(apiUrl, jsonPath, false)) {
        FILE* fp = fopen(jsonPath.c_str(), "r");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char* buffer = new char[size + 1];
            fread(buffer, 1, size, fp);
            buffer[size] = 0;
            fclose(fp);
            remove(jsonPath.c_str());

            std::string json(buffer);
            delete[] buffer;

            size_t pos = json.find("\"tag_name\":\"");
            if (pos != std::string::npos) {
                pos += 12;
                size_t end = json.find("\"", pos);
                if (end != std::string::npos) {
                    return json.substr(pos, end - pos);
                }
            }
        }
    }
    return "Latest";
}

void clearScreen() {
    consoleClear();
}

void printHeader(const std::string& currentVersion = "") {
    printf("\x1b[1;1H");
    printf("========================================");
    printf("\n     The Other Side Updater\n");
    if (!currentVersion.empty()) {
        printf("     Current System FW: %s\n", currentVersion.c_str());
    }
    printf("========================================\n");
}

bool createDirRecursive(const std::string& path) {
    std::string current = "";
    for (char c : path) {
        current += c;
        if (c == '/') {
            mkdir(current.c_str(), 0777);
        }
    }
    return true;
}

bool extractZip(const std::string& zipPath, const std::string& dest) {
    unzFile uf = unzOpen64(zipPath.c_str());
    if (!uf) return false;

    unz_global_info64 gi;
    if (unzGetGlobalInfo64(uf, &gi) != UNZ_OK) {
        unzClose(uf);
        return false;
    }

    char buf[8192];
    for (uLong i = 0; i < gi.number_entry; i++) {
        unz_file_info64 info;
        char filename[256];
        if (unzGetCurrentFileInfo64(uf, &info, filename, sizeof(filename), NULL, 0, NULL, 0) != UNZ_OK)
            break;

        std::string fullpath = dest + (dest.empty() || dest.back() == '/' ? "" : "/") + filename;

        if (filename[strlen(filename) - 1] == '/') {
            mkdir(fullpath.c_str(), 0777);
        } else {
            size_t pos = fullpath.find_last_of('/');
            if (pos != std::string::npos) {
                createDirRecursive(fullpath.substr(0, pos + 1));
            }

            if (unzOpenCurrentFile(uf) != UNZ_OK) {
                if (i + 1 < gi.number_entry) unzGoToNextFile(uf);
                continue;
            }

            FILE* out = fopen(fullpath.c_str(), "wb");
            if (out) {
                int read;
                while ((read = unzReadCurrentFile(uf, buf, sizeof(buf))) > 0) {
                    fwrite(buf, 1, read, out);
                }
                fclose(out);
            }
            unzCloseCurrentFile(uf);
        }

        if (i + 1 < gi.number_entry) {
            if (unzGoToNextFile(uf) != UNZ_OK) break;
        }
    }

    unzClose(uf);
    remove(zipPath.c_str());
    return true;
}

// ============== HELPER FUNCTIONS ==============

bool confirmUpdate(PadState& pad, const std::string& message) {
    // Clear middle area for clean message
    for (int row = 10; row <= 20; row++) {
        printf("\x1b[%d;1H                                                                 ", row);
    }

    printf("\x1b[12;1H%s", message.c_str());
    consoleUpdate(NULL);

    printf("\x1b[24;1HA = Yes, continue");
    printf("\x1b[25;1HB = No, cancel");
    consoleUpdate(NULL);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_A) {
            for (int row = 10; row <= 25; row++) {
                printf("\x1b[%d;1H                                                                 ", row);
            }
            consoleUpdate(NULL);
            return true;
        }
        if (kDown & HidNpadButton_B) {
            for (int row = 10; row <= 25; row++) {
                printf("\x1b[%d;1H                                                                 ", row);
            }
            consoleUpdate(NULL);
            return false;
        }
        svcSleepThread(100'000'000ULL);
    }
    return false;
}

// ============== UPDATE FUNCTIONS ==============

void updateAtmosphere(PadState& pad, bool withReboot = false) {
    if (!confirmUpdate(pad, "Update Atmosphere?")) return;

    clearScreen();
    printHeader();
    printf("\n\n\nDownloading Atmosphere...\n");
    consoleUpdate(NULL);

    if (downloadFile(atmosphereUrl, "/switch/ams.zip")) {
        printf("\x1b[18;1H");  // Clear progress line
        printf("Extracting...\n");
        consoleUpdate(NULL);
        extractZip("/switch/ams.zip", "/");
        printf("Atmosphere updated successfully!\n");
    } else {
        printf("Download failed! Check Wi-Fi or link.\n");
    }

    if (withReboot) {
        printf("Rebooting to apply changes...\n");
        consoleUpdate(NULL);
        svcSleepThread(3'000'000'000ULL);
        spsmInitialize();
        spsmShutdown(true);
    } else {
        printf("\nPress A to return to menu\n");
        consoleUpdate(NULL);
        while (appletMainLoop()) {
            padUpdate(&pad);
            u64 kDown = padGetButtonsDown(&pad);
            if (kDown & HidNpadButton_A) break;
            svcSleepThread(100'000'000ULL);
        }
    }
}

void updateHekate(PadState& pad, bool withReboot = false) {
    if (!confirmUpdate(pad, "Update Hekate?")) return;

    clearScreen();
    printHeader();
    printf("\n\n\nDownloading Hekate...\n");
    consoleUpdate(NULL);

    if (downloadFile(hekateUrl, "/switch/hekate.zip")) {
        printf("\x1b[18;1H");  // Clear progress
        printf("Extracting...\n");
        consoleUpdate(NULL);
        extractZip("/switch/hekate.zip", "/");
        rename("/bootloader/hekate_ctcaer.bin", "/atmosphere/reboot_payload.bin");
        printf("Hekate updated successfully!\n");
    } else {
        printf("Download failed! Check Wi-Fi or link.\n");
    }

    if (withReboot) {
        printf("Rebooting to apply changes...\n");
        consoleUpdate(NULL);
        svcSleepThread(3'000'000'000ULL);
        spsmInitialize();
        spsmShutdown(true);
    } else {
        printf("\nPress A to return to menu\n");
        consoleUpdate(NULL);
        while (appletMainLoop()) {
            padUpdate(&pad);
            u64 kDown = padGetButtonsDown(&pad);
            if (kDown & HidNpadButton_A) break;
            svcSleepThread(100'000'000ULL);
        }
    }
}

void updateCFW(PadState& pad) {
    if (!confirmUpdate(pad, "Update Atmosphere + Hekate together?")) return;

    clearScreen();
    printHeader();
    printf("\n\n\nUpdating Atmosphere + Hekate...\n");
    consoleUpdate(NULL);

    updateAtmosphere(pad, false);
    updateHekate(pad, false);

    printf("Both updates complete!\n");
    printf("Rebooting to apply changes...\n");
    consoleUpdate(NULL);
    svcSleepThread(3'000'000'000ULL);
    spsmInitialize();
    spsmShutdown(true);
}

void updateFirmware(PadState& pad) {
    int selected = 0;

    clearScreen();  // Full clear when entering firmware menu
    printHeader();
    printf("\n\nSelect firmware pack:\n\n");

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_StickLUp || kDown & HidNpadButton_Up)
            selected = (selected - 1 + (int)firmwareUrls.size()) % firmwareUrls.size();
        if (kDown & HidNpadButton_StickLDown || kDown & HidNpadButton_Down)
            selected = (selected + 1) % firmwareUrls.size();

        // Clear only the list area (rows 10-20) for smooth scrolling
        for (int row = 10; row <= 20; row++) {
            printf("\x1b[%d;1H                                                                 ", row);
        }

        for (int i = 0; i < (int)firmwareNames.size(); i++) {
            printf("\x1b[%d;1H    %s %s", 10 + i, (i == selected ? ">" : " "), firmwareNames[i].c_str());
        }

        printf("\x1b[24;1HA = Download selected pack");
        printf("\x1b[25;1HB = Cancel / Back to main menu");
        consoleUpdate(NULL);

        if (kDown & HidNpadButton_A) {
            if (!confirmUpdate(pad, "Download " + firmwareNames[selected] + "?")) continue;

            clearScreen();
            printHeader();
            printf("\n\n\nDownloading %s...\n", firmwareNames[selected].c_str());
            consoleUpdate(NULL);

            std::string url = firmwareUrls[selected];
            std::string name = firmwareNames[selected];
            std::string zipPath = "/switch/firmware_pack.zip";

            if (downloadFile(url, zipPath)) {
                printf("\x1b[18;1H");  // Clear progress
                std::string extractDir = "/firmware/" + name + "/";
                mkdir("/firmware", 0777);
                createDirRecursive(extractDir);
                printf("Extracting...\n");
                consoleUpdate(NULL);
                extractZip(zipPath, extractDir);
                printf("\nFirmware pack ready in:\n%s\n", extractDir.c_str());
                printf("Launch Daybreak.nro and select this folder.\n");
            } else {
                printf("\nDownload failed!\n");
            }

            printf("\nPress A to return to firmware list\n");
            consoleUpdate(NULL);
            while (appletMainLoop()) {
                padUpdate(&pad);
                if (padGetButtonsDown(&pad) & HidNpadButton_A) break;
                svcSleepThread(100'000'000ULL);
            }
        }

        if (kDown & HidNpadButton_B) {
            clearScreen();  // Full clear on cancel
            return;
        }
    }
}

void updateAppSelf(PadState& pad) {
    if (!confirmUpdate(pad, "Update the app itself?")) return;

    clearScreen();
    printHeader();
    printf("\n\n\nDownloading latest app version...\n");
    consoleUpdate(NULL);

    const std::string appPath = "/switch/the_other_side_updater.nro";

    if (downloadFile(appUpdateUrl, appPath)) {
        printf("\x1b[18;1H");  // Clear progress
        printf("Update successful!\n");
        printf("New version downloaded.\n");
        printf("Exit (+) and relaunch the app\n");
        printf("to use the updated version.\n");
    } else {
        printf("Update failed! Check internet or link.\n");
    }

    printf("\nPress A to return to menu\n");
    consoleUpdate(NULL);
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_A) break;
        svcSleepThread(100'000'000ULL);
    }
}

// ============== MAIN ==============

int main(int argc, char** argv) {
    consoleInit(NULL);
    socketInitializeDefault();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    std::string currentVersion = "Unknown";
    setsysInitialize();
    SetSysFirmwareVersion ver;
    if (R_SUCCEEDED(setsysGetFirmwareVersion(&ver))) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u.%u.%u", ver.major, ver.minor, ver.micro);
        currentVersion = buf;
    }
    setsysExit();

    std::string atmosVersion = fetchLatestVersion(atmosApiUrl);
    std::string hekateVersion = fetchLatestVersion(hekateApiUrl);

    int selected = 0;
    const std::vector<std::string> options = {
        "Update CFW (Atmos + Hekate)",
        "Update Atmosphere (" + atmosVersion + ")",
        "Update Hekate (" + hekateVersion + ")",
        "Update Firmware",
        "Update App",
        "Exit & Reboot"
    };

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_StickLUp || kDown & HidNpadButton_Up)
            selected = (selected - 1 + (int)options.size()) % options.size();
        if (kDown & HidNpadButton_StickLDown || kDown & HidNpadButton_Down)
            selected = (selected + 1) % options.size();

        clearScreen();
        printHeader(currentVersion);
        printf("\n\n\nUse Up/Down to select • A to confirm • + to exit\n\n");

        for (int i = 0; i < (int)options.size(); i++) {
            printf("%s %s\n", (i == selected ? ">" : " "), options[i].c_str());
        }
        printf("\n");
        consoleUpdate(NULL);

        if (kDown & HidNpadButton_A) {
            if (selected == 0) updateCFW(pad);
            else if (selected == 1) updateAtmosphere(pad);
            else if (selected == 2) updateHekate(pad);
            else if (selected == 3) updateFirmware(pad);
            else if (selected == 4) updateAppSelf(pad);
            else if (selected == 5) {
                clearScreen();
                printHeader(currentVersion);
                printf("\n\n\nRebooting (full power cycle)...\n");
                printf("Switch will turn off and boot back into CFW.\n");
                consoleUpdate(NULL);
                svcSleepThread(3'000'000'000ULL);
                spsmInitialize();
                spsmShutdown(true);
            }
        }

        if (kDown & HidNpadButton_Plus) break;
        svcSleepThread(50'000'000ULL);
    }

    socketExit();
    consoleExit(NULL);
    return 0;
}
