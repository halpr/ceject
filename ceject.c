/*
 * Ejectd - External Drive Ejector
 * Author: Filipe Soares
 * GitHub: https://github.com/halpr
 * License: MIT
 * Version: 1.0.0 (C Port)
 * Description: Safe ejection tool for external drives
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>

// Colors and styling
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[0;34m"
#define CYAN "\033[0;36m"
#define MAGENTA "\033[0;35m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define NC "\033[0m"

// Icons
#define ICON_DRIVE "💾"
#define ICON_USB "🔌"
#define ICON_MOUNTED "📌"
#define ICON_UNMOUNTED "⭕"
#define ICON_SUCCESS "✅"
#define ICON_ERROR "❌"
#define ICON_WARNING "⚠️"
#define ICON_EJECT "⏏️"

#define MAX_DRIVES 32
#define MAX_PATH 256
#define MAX_LINE 1024
#define MAX_PARTITIONS 16

typedef struct {
    char path[MAX_PATH];
    char size[64];
    char model[128];
    char vendor[128];
    char transport[32];
    int mount_count;
    char mountpoints[8][MAX_PATH];
} DriveInfo;

// Display header
void show_header(void) {
    system("clear");
    printf("\n%s%s%sEjectd %s External Drive Ejector%s\n", 
           BOLD, MAGENTA, ICON_EJECT, ICON_EJECT, NC);
    printf("%sSafe removal tool for external drives%s\n\n", DIM, NC);
}

// Execute command and get output
char* exec_cmd(const char* cmd) {
    static char buffer[MAX_LINE * 10];
    buffer[0] = '\0';
    
    FILE* fp = popen(cmd, "r");
    if (fp == NULL) return buffer;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp) != NULL) {
        strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
    }
    
    pclose(fp);
    return buffer;
}

// Get root drive
void get_root_drive(char* root_drive, size_t size) {
    char cmd[MAX_LINE];
    snprintf(cmd, sizeof(cmd), "lsblk -no PKNAME \"$(findmnt -n -o SOURCE /)\" 2>/dev/null");
    char* result = exec_cmd(cmd);
    
    // Remove newline
    char* newline = strchr(result, '\n');
    if (newline) *newline = '\0';
    
    strncpy(root_drive, result, size - 1);
    root_drive[size - 1] = '\0';
}

// Get drive information
void get_drive_info(const char* drive, DriveInfo* info) {
    char cmd[MAX_LINE];
    
    // Initialize
    strcpy(info->path, drive);
    info->mount_count = 0;
    
    // Get size, model, vendor, transport
    snprintf(cmd, sizeof(cmd), "lsblk -no SIZE,MODEL,VENDOR,TRAN \"%s\" 2>/dev/null | head -1", drive);
    char* result = exec_cmd(cmd);
    
    sscanf(result, "%63s %127s %127s %31s", 
           info->size, info->model, info->vendor, info->transport);
    
    // Get mount points
    snprintf(cmd, sizeof(cmd), "lsblk -no MOUNTPOINT \"%s\" 2>/dev/null", drive);
    result = exec_cmd(cmd);
    
    char* line = strtok(result, "\n");
    while (line != NULL && info->mount_count < 8) {
        if (line[0] == '/') {
            strncpy(info->mountpoints[info->mount_count], line, MAX_PATH - 1);
            info->mountpoints[info->mount_count][MAX_PATH - 1] = '\0';
            info->mount_count++;
        }
        line = strtok(NULL, "\n");
    }
}

// Get all external drives
int get_drives(DriveInfo drives[], int max_drives) {
    char root_drive[64] = "";
    get_root_drive(root_drive, sizeof(root_drive));
    
    char cmd[MAX_LINE];
    snprintf(cmd, sizeof(cmd), 
             "lsblk -ndo NAME,TYPE | awk -v rd=\"%s\" '$2==\"disk\" && $1!=rd {print \"/dev/\"$1}'",
             root_drive);
    
    char* result = exec_cmd(cmd);
    
    int count = 0;
    char* line = strtok(result, "\n");
    
    while (line != NULL && count < max_drives) {
        get_drive_info(line, &drives[count]);
        count++;
        line = strtok(NULL, "\n");
    }
    
    return count;
}

// Display drives
void show_drives(DriveInfo drives[], int count) {
    show_header();
    
    if (count == 0) {
        printf("%s%s No external drives found.%s\n\n", RED, ICON_ERROR, NC);
        printf("Press Enter to exit...");
        getchar();
        exit(1);
    }
    
    printf("%s%sAvailable Drives:%s\n", BOLD, GREEN, NC);
    printf("%s────────────────────────────────────────────────────────────%s\n\n", DIM, NC);
    
    for (int i = 0; i < count; i++) {
        DriveInfo* drive = &drives[i];
        
        // Build friendly name
        char friendly_name[256];
        if (strlen(drive->vendor) > 0 || strlen(drive->model) > 0) {
            snprintf(friendly_name, sizeof(friendly_name), "%s%s%s", 
                    strlen(drive->vendor) > 0 ? drive->vendor : "",
                    strlen(drive->vendor) > 0 ? " " : "",
                    strlen(drive->model) > 0 ? drive->model : "Unknown Drive");
        } else {
            strcpy(friendly_name, "Unknown Drive");
        }
        
        // Mount info
        const char* mount_info;
        char mount_extra[64] = "";
        
        if (drive->mount_count > 0) {
            mount_info = GREEN ICON_MOUNTED " Mounted" NC;
            if (drive->mount_count > 3) {
                snprintf(mount_extra, sizeof(mount_extra), " (%d locations)", drive->mount_count);
            }
        } else {
            mount_info = DIM ICON_UNMOUNTED " Not mounted" NC;
        }
        
        // Connection type
        const char* conn_icon = ICON_USB;
        const char* conn_type = "USB";
        
        if (strcmp(drive->transport, "sata") == 0) {
            conn_icon = "💿";
            conn_type = "SATA";
        } else if (strcmp(drive->transport, "nvme") == 0) {
            conn_icon = "⚡";
            conn_type = "NVMe";
        }
        
        // Display drive info
        printf("%s%s[%d]%s %s %s%s%s\n", BOLD, YELLOW, i + 1, NC, conn_icon, BOLD, friendly_name, NC);
        printf("    %s├─%s %sDevice:%s %s\n", DIM, NC, CYAN, NC, drive->path);
        printf("    %s├─%s %sSize:%s %s\n", DIM, NC, CYAN, NC, drive->size);
        printf("    %s├─%s %sType:%s %s\n", DIM, NC, CYAN, NC, conn_type);
        printf("    %s└─%s %sStatus:%s %s%s\n", DIM, NC, CYAN, NC, mount_info, mount_extra);
        
        // Show mount points if mounted and count <= 3
        if (drive->mount_count > 0 && drive->mount_count <= 3) {
            for (int j = 0; j < drive->mount_count; j++) {
                printf("       %s→%s %s\n", DIM, NC, drive->mountpoints[j]);
            }
        }
        printf("\n");
    }
    
    printf("%s────────────────────────────────────────────────────────────%s\n", DIM, NC);
}

// Unmount drive
bool unmount_drive(const char* drive_path) {
    char cmd[MAX_LINE];
    
    show_header();
    printf("%s%s%s Selected: %s%s\n\n", BOLD, YELLOW, ICON_WARNING, drive_path, NC);
    printf("%s%s Unmounting all partitions...%s\n\n", CYAN, ICON_DRIVE, NC);
    
    // Get all partitions
    snprintf(cmd, sizeof(cmd), "lsblk -lno NAME \"%s\" | tail -n +2", drive_path);
    char* result = exec_cmd(cmd);
    
    bool unmount_failed = false;
    char* partition = strtok(result, "\n");
    
    while (partition != NULL) {
        char part_path[MAX_PATH];
        snprintf(part_path, sizeof(part_path), "/dev/%s", partition);
        
        // Check if mounted
        snprintf(cmd, sizeof(cmd), "lsblk -no MOUNTPOINT \"%s\" 2>/dev/null", part_path);
        char* mountpoint = exec_cmd(cmd);
        
        // Remove newline
        char* newline = strchr(mountpoint, '\n');
        if (newline) *newline = '\0';
        
        if (strlen(mountpoint) > 0) {
            printf("  %s→%s Unmounting %s (%s)...\n", DIM, NC, part_path, mountpoint);
            
            snprintf(cmd, sizeof(cmd), "udisksctl unmount -b \"%s\" &>/dev/null", part_path);
            int ret = system(cmd);
            
            if (ret == 0) {
                printf("    %s%s Success%s\n", GREEN, ICON_SUCCESS, NC);
            } else {
                printf("    %s%s Failed%s\n", RED, ICON_ERROR, NC);
                unmount_failed = true;
            }
        }
        
        partition = strtok(NULL, "\n");
    }
    
    if (unmount_failed) {
        printf("\n%s%s Some partitions failed to unmount.%s\n", RED, ICON_ERROR, NC);
        printf("%s%s The drive may still be in use.%s\n\n", YELLOW, ICON_WARNING, NC);
        printf("Press Enter to continue...");
        getchar();
        return false;
    }
    
    // Power off the drive
    printf("\n%s%s Powering off the drive...%s\n\n", CYAN, ICON_EJECT, NC);
    snprintf(cmd, sizeof(cmd), "udisksctl power-off -b \"%s\" &>/dev/null", drive_path);
    int ret = system(cmd);
    
    if (ret == 0) {
        printf("%s%s Drive %s has been safely ejected!%s\n", GREEN, ICON_SUCCESS, drive_path, NC);
        printf("%s%s You can now safely remove the drive.%s\n\n", GREEN, ICON_SUCCESS, NC);
    } else {
        printf("%s%s Failed to power off the drive.%s\n\n", RED, ICON_ERROR, NC);
    }
    
    printf("Press Enter to continue...");
    getchar();
    return true;
}

int main(void) {
    DriveInfo drives[MAX_DRIVES];
    int drive_count;
    char input[16];
    
    while (true) {
        drive_count = get_drives(drives, MAX_DRIVES);
        show_drives(drives, drive_count);
        
        printf("\n%s%sOptions:%s\n", BOLD, CYAN, NC);
        printf("  %s[1-%d]%s Select a drive to eject\n", YELLOW, drive_count, NC);
        printf("  %s[r]%s Refresh drive list\n", YELLOW, NC);
        printf("  %s[q]%s Quit\n\n", YELLOW, NC);
        printf("%s%sYour choice: %s", BOLD, GREEN, NC);
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        
        // Remove newline
        char* newline = strchr(input, '\n');
        if (newline) *newline = '\0';
        
        // Convert to lowercase
        for (int i = 0; input[i]; i++) {
            input[i] = tolower(input[i]);
        }
        
        if (strcmp(input, "q") == 0) {
            printf("\n%sGoodbye!%s\n", CYAN, NC);
            break;
        } else if (strcmp(input, "r") == 0) {
            continue;
        } else {
            int choice = atoi(input);
            if (choice >= 1 && choice <= drive_count) {
                unmount_drive(drives[choice - 1].path);
            } else {
                printf("\n%s%s Invalid selection.%s\n", RED, ICON_ERROR, NC);
                sleep(2);
            }
        }
    }
    
    return 0;
}
