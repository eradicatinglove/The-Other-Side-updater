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

// ================== CONFIG ==================

const std::string atmosApiUrl = "https://api.github.com/repos/eradicatinglove/Atmosphere/releases/latest";
const std::string hekateApiUrl = "https://api.github.com/repos/eradicatinglove/hekate/releases/latest";
const std::string appVersion  = "v1.0.0";  // bump this when you release

const std::string atmosphereUrl = "https://github.com/eradicatinglove/Atmosphere/releases/download/v1.10.1/TheOtherSide_ams_1.10.1.zip";
const std::string hekateUrl     = "https://github.com/eradicatinglove/hekate/releases/download/v6.4.2/TheOtherSide_hekate_6.4.2.zip";
const std::string appUpdateUrl  = "https://github.com/eradicatinglove/the-other-side-updater/releases/latest/download/the_other_side_updater.nro";

const std::vector<std::string> firmwareUrls = {
    "https://github.com/eradicatinglove/The-Other-Side-firmware/releases/download/fw-21.1.0/Firmware_21.1.0.zip",
    "https://github.com/eradicatinglove/The-Other-Side-firmware/releases/download/fw-20.5.0/20.5.0.zip",
};

const std::vector<std::string> firmwareNames = {
    "Firmware (21.1.0)",
    "Firmware (20.5.0)",
};

// ================== UI HELPERS ==================

static void consoleFlush() {
    consoleUpdate(NULL);
}

static void fullClear() {
    // Strong ANSI clear (more reliable than consoleClear alone with cursor-positioned printing)
    printf("\x1b[2J\x1b[H");
    consoleFlush();
}

static void clearRows(int startRow, int endRow) {
    for (int row = startRow; row <= endRow; row++) {
        printf("\x1b[%d;1H%*s", row, 80, "");
    }
}

// Header always uses rows 1-4
static void printHeader(const std::string& currentVersion = "") {
    printf("\x1b[1;1H");
    printf("========================================\n");
    printf("     The Other Side Updater %s\n", appVersion.c_str());
    if (!currentVersion.empty()) {
        printf("     Current System FW: %s\n", currentVersion.c_str());
    } else {
        printf("\n");
    }
    printf("========================================\n");
}

// Clears the area below the header, then prints the main menu.
static void drawMainMenu(const std::string& currentVersion,
                         const std::vector<std::string>& options,
                         int selected) {
    // Keep header stable, wipe the rest so old menu text can NEVER linger.
    printHeader(currentVersion);
    clearRows(6, 28);

    printf("\x1b[6;1HUse Up/Down to select   A = confirm   + = exit\n\n");

    int baseRow = 8;
    for (int i = 0; i < (int)options.size(); i++) {
        printf("\x1b[%d;1H%s %s", baseRow + i, (i == selected ? ">" : " "), options[i].c_str());
    }
    consoleFlush();
}

// ================== DOWNLOAD / ZIP ==================

static size_t write_data(void* ptr, size_t size, size_t nmemb, void* stream) {
    return fwrite(ptr, size, nmemb, (FILE*)stream);
}

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp; (void)ultotal; (void)ulnow;
    if (dltotal > 0) {
        int percent = (int)((dlnow * 100LL) / dltotal);
        printf("\x1b[18;1HDownloading... %d%% (%ld / %ld bytes)%*s",
               percent, (long)dlnow, (long)dltotal, 10, "");
        consoleFlush();
    }
    return 0;
}

bool downloadFile(const std::string& url, const std::string& path, bool showProgress = true) {
    appletSetAutoSleepDisabled(true);

    CURL* curl = curl_easy_init();
    if (!curl) {
        appletSetAutoSleepDisabled(false);
        return false;
    }

    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        appletSetAutoSleepDisabled(false);
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

    appletSetAutoSleepDisabled(false);
    return (res == CURLE_OK);
}

std::string fetchLatestVersion(const std::string& apiUrl) {
    const std::string jsonPath = "/switch/temp.json";
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

bool createDirRecursive(const std::string& path) {
    std::string current;
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

// ================== CONFIRM ==================

bool confirmUpdate(PadState& pad, const std::string& message) {
    // wipe middle content area
    clearRows(10, 20);

    printf("\x1b[12;1H%s", message.c_str());
    printf("\x1b[24;1HA = Yes, continue");
    printf("\x1b[25;1HB = No, cancel");
    consoleFlush();

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_A) {
            clearRows(10, 25);
            consoleFlush();
            return true;
        }
        if (kDown & HidNpadButton_B) {
            clearRows(10, 25);
            consoleFlush();
            return false;
        }
        svcSleepThread(100'000'000ULL);
    }
    return false;
}

// ================== UPDATE FUNCTIONS ==================

void updateAtmosphere(PadState& pad) {
    if (!confirmUpdate(pad, "Update Atmosphere?")) return;

    fullClear();
    printHeader();
    printf("\n\nDownloading Atmosphere...\n");
    consoleFlush();

    if (downloadFile(atmosphereUrl, "/switch/ams.zip")) {
        printf("\x1b[18;1H%*s", 80, ""); // clear progress line
        printf("\nExtracting...\n");
        consoleFlush();
        extractZip("/switch/ams.zip", "/");
        printf("Atmosphere updated successfully!\n");
    } else {
        printf("Download failed! Check Wi-Fi or link.\n");
    }

    printf("\nPress A to return to menu\n");
    consoleFlush();
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_A) break;
        svcSleepThread(100'000'000ULL);
    }
}

void updateHekate(PadState& pad) {
    if (!confirmUpdate(pad, "Update Hekate?")) return;

    fullClear();
    printHeader();
    printf("\n\nDownloading Hekate...\n");
    consoleFlush();

    if (downloadFile(hekateUrl, "/switch/hekate.zip")) {
        printf("\x1b[18;1H%*s", 80, "");
        printf("\nExtracting...\n");
        consoleFlush();
        extractZip("/switch/hekate.zip", "/");
        rename("/bootloader/hekate_ctcaer.bin", "/atmosphere/reboot_payload.bin");
        printf("Hekate updated successfully!\n");
    } else {
        printf("Download failed! Check Wi-Fi or link.\n");
    }

    printf("\nPress A to return to menu\n");
    consoleFlush();
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_A) break;
        svcSleepThread(100'000'000ULL);
    }
}

void updateCFW(PadState& pad) {
    if (!confirmUpdate(pad, "Update Atmosphere + Hekate together?")) return;

    // Do them back-to-back without extra confirmations
    fullClear();
    printHeader();
    printf("\n\nUpdating Atmosphere...\n");
    consoleFlush();

    if (downloadFile(atmosphereUrl, "/switch/ams.zip")) {
        printf("\x1b[18;1H%*s", 80, "");
        printf("\nExtracting Atmosphere...\n");
        consoleFlush();
        extractZip("/switch/ams.zip", "/");
        printf("Atmosphere done.\n");
    } else {
        printf("Atmosphere download failed!\n");
    }

    printf("\nUpdating Hekate...\n");
    consoleFlush();
    if (downloadFile(hekateUrl, "/switch/hekate.zip")) {
        printf("\x1b[18;1H%*s", 80, "");
        printf("\nExtracting Hekate...\n");
        consoleFlush();
        extractZip("/switch/hekate.zip", "/");
        rename("/bootloader/hekate_ctcaer.bin", "/atmosphere/reboot_payload.bin");
        printf("Hekate done.\n");
    } else {
        printf("Hekate download failed!\n");
    }

    printf("\nBoth updates complete!\n");
    printf("Press A to reboot now, or B to go back.\n");
    consoleFlush();

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_A) {
            spsmInitialize();
            spsmShutdown(true);
        }
        if (kDown & HidNpadButton_B) break;
        svcSleepThread(100'000'000ULL);
    }
}

void updateFirmware(PadState& pad) {
    int selected = 0;

    fullClear();
    printHeader();
    clearRows(6, 28);

    printf("\x1b[6;1HSelect firmware pack:\n");
    printf("\x1b[24;1HA = Download selected pack");
    printf("\x1b[25;1HB = Back");
    consoleFlush();

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if ((kDown & HidNpadButton_StickLUp) || (kDown & HidNpadButton_Up))
            selected = (selected - 1 + (int)firmwareUrls.size()) % (int)firmwareUrls.size();
        if ((kDown & HidNpadButton_StickLDown) || (kDown & HidNpadButton_Down))
            selected = (selected + 1) % (int)firmwareUrls.size();

        // redraw only list area
        clearRows(8, 20);

        for (int i = 0; i < (int)firmwareNames.size(); i++) {
            printf("\x1b[%d;1H%s %s", 8 + i, (i == selected ? ">" : " "), firmwareNames[i].c_str());
        }
        consoleFlush();

        if (kDown & HidNpadButton_A) {
            if (!confirmUpdate(pad, "Download " + firmwareNames[selected] + "?")) {
                // restore firmware menu instructions after confirm clears
                printHeader();
                printf("\x1b[6;1HSelect firmware pack:\n");
                printf("\x1b[24;1HA = Download selected pack");
                printf("\x1b[25;1HB = Back");
                consoleFlush();
                continue;
            }

            fullClear();
            printHeader();
            printf("\n\nDownloading %s...\n", firmwareNames[selected].c_str());
            consoleFlush();

            mkdir("/firmware", 0777);

            std::string safeName = firmwareNames[selected];
            for (char& c : safeName) {
                if (c == ' ' || c == '(' || c == ')') c = '_';
            }

            std::string zipPath = "/firmware/" + safeName + ".zip";

            if (downloadFile(firmwareUrls[selected], zipPath)) {
                printf("\x1b[18;1H%*s", 80, "");
                printf("\nFirmware ZIP downloaded successfully!\n");
                printf("Saved to:\n%s\n\n", zipPath.c_str());
                printf("Open Daybreak and select this ZIP.\n");
            } else {
                printf("\nDownload failed!\n");
            }

            printf("\nPress A to return to firmware list\n");
            consoleFlush();
            while (appletMainLoop()) {
                padUpdate(&pad);
                if (padGetButtonsDown(&pad) & HidNpadButton_A) break;
                svcSleepThread(100'000'000ULL);
            }

            // return to firmware list (clean redraw)
            fullClear();
            printHeader();
            clearRows(6, 28);
            printf("\x1b[6;1HSelect firmware pack:\n");
            printf("\x1b[24;1HA = Download selected pack");
            printf("\x1b[25;1HB = Back");
            consoleFlush();
        }

        if (kDown & HidNpadButton_B) {
            // Just return; main menu redraw will wipe rows 6-28 and the text will be gone.
            return;
        }
    }
}

void updateAppSelf(PadState& pad) {
    if (!confirmUpdate(pad, "Update the app itself?")) return;

    fullClear();
    printHeader();
    printf("\n\nDownloading latest app version...\n");
    consoleFlush();

    const std::string appPath = "/switch/the_other_side_updater.nro";

    if (downloadFile(appUpdateUrl, appPath)) {
        printf("\x1b[18;1H%*s", 80, "");
        printf("\nUpdate successful!\n");
        printf("New version downloaded.\n");
        printf("Press + to exit the app\n");
        printf("Then relaunch from Homebrew Menu\n");
        consoleFlush();

        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
            svcSleepThread(100'000'000ULL);
        }
    } else {
        printf("\nUpdate failed!\n");
        printf("Check internet connection.\n");
        printf("\nPress A to return to menu\n");
        consoleFlush();

        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_A) break;
            svcSleepThread(100'000'000ULL);
        }
    }
}

// ================== MAIN ==================

int main(int argc, char** argv) {
    (void)argc; (void)argv;

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

    // Versions from GitHub (your repos)
    std::string atmosVersion  = fetchLatestVersion(atmosApiUrl);
    std::string hekateVersion = fetchLatestVersion(hekateApiUrl);

    int selected = 0;
    std::vector<std::string> options = {
        "Update CFW (Atmos + Hekate)",
        "Update Atmosphere (" + atmosVersion + ")",
        "Update Hekate (" + hekateVersion + ")",
        "Update Firmware",
        "Update App (" + appVersion + ")",
        "Exit & Reboot"
    };

    fullClear();
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if ((kDown & HidNpadButton_StickLUp) || (kDown & HidNpadButton_Up))
            selected = (selected - 1 + (int)options.size()) % (int)options.size();
        if ((kDown & HidNpadButton_StickLDown) || (kDown & HidNpadButton_Down))
            selected = (selected + 1) % (int)options.size();

        // Always redraw main menu cleanly (this is what prevents leftover firmware text)
        drawMainMenu(currentVersion, options, selected);

        if (kDown & HidNpadButton_A) {
            if (selected == 0) updateCFW(pad);
            else if (selected == 1) updateAtmosphere(pad);
            else if (selected == 2) updateHekate(pad);
            else if (selected == 3) updateFirmware(pad);
            else if (selected == 4) updateAppSelf(pad);
            else if (selected == 5) {
                fullClear();
                printHeader(currentVersion);
                printf("\n\nRebooting (full power cycle)...\n");
                printf("Switch will turn off and boot back into CFW.\n");
                consoleFlush();
                svcSleepThread(3'000'000'000ULL);
                spsmInitialize();
                spsmShutdown(true);
            }

            // After returning from any submenu, force a clean redraw immediately
            fullClear();
        }

        if (kDown & HidNpadButton_Plus) break;
        svcSleepThread(50'000'000ULL);
    }

    socketExit();
    consoleExit(NULL);
    return 0;
}

