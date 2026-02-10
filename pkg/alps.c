#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>

#define INSTALLED_DB "/var/lib/alps/installed"
#define PKG_REPO     "/var/alps/package/"
#define METADATA_DIR "/var/alps/pkg/"

// Check if package is already in the system
int is_installed(const char *pkg) {
    FILE *f = fopen(INSTALLED_DB, "r");
    if (!f) return 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, pkg) == 0) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

// Check for dependencies in /var/alps/pkg/[pkgname].dep
void check_deps(const char *pkg) {
    char dep_path[256];
    snprintf(dep_path, sizeof(dep_path), "%s%s.dep", METADATA_DIR, pkg);
    
    FILE *f = fopen(dep_path, "r");
    if (!f) return; // No dependency file found, assume none

    char dep[128];
    while (fgets(dep, sizeof(dep), f)) {
        dep[strcspn(dep, "\n")] = 0;
        if (!is_installed(dep)) {
            printf("(!) You might need to install %s, btw.\n", dep);
        }
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[1], "install") != 0) {
        printf("alps v0.2\nUsage: alps install <package>\n");
        return 1;
    }

    char *target_pkg = argv[2];
    char source_path[512];
    
    // Construct the file path: file:///var/alps/package/pkg.tar.gz
    snprintf(source_path, sizeof(source_path), "file://%s%s.tar.gz", PKG_REPO, target_pkg);

    printf("alps: searching for %s...\n", target_pkg);

    // 1. Dependency check from /var/alps/pkg/
    check_deps(target_pkg);

    // 2. "Fetch" using libcurl (handles file:// protocol perfectly)
    CURL *curl = curl_easy_init();
    if (curl) {
        char out_name[256];
        snprintf(out_name, sizeof(out_name), "%s.tar.gz", target_pkg);
        FILE *fp = fopen(out_name, "wb");

        curl_easy_setopt(curl, CURLOPT_URL, source_path);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        
        CURLcode res = curl_easy_perform(curl);
        fclose(fp);

        if (res == CURLE_OK) {
            printf("alps: %s successfully pulled from local repo.\n", target_pkg);
            
            // 3. The Actual "Install" (Extraction)
            char untar_cmd[512];
            // Slackware style: extract to root or current dir
            snprintf(untar_cmd, sizeof(untar_cmd), "tar -xzf %s.tar.gz", target_pkg);
            if (system(untar_cmd) == 0) {
                printf("alps: %s extracted to system.\n", target_pkg);
                
                // Update DB
                FILE *db = fopen(INSTALLED_DB, "a");
                fprintf(db, "%s\n", target_pkg);
                fclose(db);
            }
        } else {
            printf("alps: Package %s not found in %s\n", target_pkg, PKG_REPO);
        }
        curl_easy_cleanup(curl);
    }

    return 0;
}
