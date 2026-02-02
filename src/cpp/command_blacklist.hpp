#pragma once

#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <algorithm>
#include <cctype>

// Command blacklist for security
// This prevents execution of dangerous system commands
class CommandBlacklist {
public:
    static CommandBlacklist& instance() {
        static CommandBlacklist inst;
        return inst;
    }

    // Check if a command is blacklisted
    bool is_blacklisted(const std::string& command) const {
        std::string cmd_name = extract_command_name(command);
        return blacklisted_commands_.count(cmd_name) > 0;
    }

    // Check if command contains dangerous patterns
    bool has_dangerous_patterns(const std::string& command) const {
        for (const auto& pattern : dangerous_patterns_) {
            if (command.find(pattern) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    // Get list of blacklisted commands (for error messages)
    std::string get_blacklisted_info(const std::string& command) const {
        std::string cmd_name = extract_command_name(command);
        if (blacklisted_commands_.count(cmd_name)) {
            return "Command '" + cmd_name + "' is blacklisted for security reasons.";
        }
        if (has_dangerous_patterns(command)) {
            return "Command contains dangerous patterns (pipes, redirects, command substitution).";
        }
        return "Command validation failed.";
    }

private:
    CommandBlacklist() {
        initialize_blacklist();
    }

    // Extract the base command name (first word, handles paths)
    std::string extract_command_name(const std::string& command) const {
        std::string trimmed = trim(command);
        size_t space_pos = trimmed.find_first_of(" \t");
        std::string cmd = trimmed.substr(0, space_pos);

        // Extract just the filename from path
        size_t slash_pos = cmd.find_last_of("/\\");
        if (slash_pos != std::string::npos) {
            cmd = cmd.substr(slash_pos + 1);
        }

        return cmd;
    }

    std::string trim(const std::string& str) const {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    void initialize_blacklist() {
        // System modification commands
        blacklisted_commands_.insert("rm");
        blacklisted_commands_.insert("rmdir");
        blacklisted_commands_.insert("shred");
        blacklisted_commands_.insert("wipe");

        // User management
        blacklisted_commands_.insert("useradd");
        blacklisted_commands_.insert("userdel");
        blacklisted_commands_.insert("usermod");
        blacklisted_commands_.insert("passwd");
        blacklisted_commands_.insert("chpasswd");

        // Group management
        blacklisted_commands_.insert("groupadd");
        blacklisted_commands_.insert("groupdel");
        blacklisted_commands_.insert("groupmod");

        // Permission modification
        blacklisted_commands_.insert("chmod");
        blacklisted_commands_.insert("chown");
        blacklisted_commands_.insert("chgrp");

        // System commands
        blacklisted_commands_.insert("su");
        blacklisted_commands_.insert("sudo");
        blacklisted_commands_.insert("doas");
        blacklisted_commands_.insert("pkexec");

        // Package managers (could be used to install malware)
        blacklisted_commands_.insert("apt");
        blacklisted_commands_.insert("apt-get");
        blacklisted_commands_.insert("dnf");
        blacklisted_commands_.insert("yum");
        blacklisted_commands_.insert("pacman");
        blacklisted_commands_.insert("zypper");
        blacklisted_commands_.insert("emerge");
        blacklisted_commands_.insert("flatpak");
        blacklisted_commands_.insert("snap");

        // System services
        blacklisted_commands_.insert("systemctl");
        blacklisted_commands_.insert("service");
        blacklisted_commands_.insert("init");
        blacklisted_commands_.insert("telinit");
        blacklisted_commands_.insert("shutdown");
        blacklisted_commands_.insert("reboot");
        blacklisted_commands_.insert("poweroff");
        blacklisted_commands_.insert("halt");

        // Network manipulation
        blacklisted_commands_.insert("iptables");
        blacklisted_commands_.insert("nft");
        blacklisted_commands_.insert("ufw");
        blacklisted_commands_.insert("firewall-cmd");
        blacklisted_commands_.insert("netstat");
        blacklisted_commands_.insert("ss");
        blacklisted_commands_.insert("tcpdump");
        blacklisted_commands_.insert("wireshark");

        // Disk manipulation
        blacklisted_commands_.insert("fdisk");
        blacklisted_commands_.insert("parted");
        blacklisted_commands_.insert("mkfs");
        blacklisted_commands_.insert("dd");
        blacklisted_commands_.insert("mount");
        blacklisted_commands_.insert("umount");

        // Kernel modules
        blacklisted_commands_.insert("modprobe");
        blacklisted_commands_.insert("insmod");
        blacklisted_commands_.insert("rmmod");
        blacklisted_commands_.insert("lsmod");

        // Boot configuration
        blacklisted_commands_.insert("grub-install");
        blacklisted_commands_.insert("update-grub");
        blacklisted_commands_.insert("efibootmgr");

        // Cryptographic manipulation
        blacklisted_commands_.insert("cryptsetup");
        blacklisted_commands_.insert("openssl"); // Can be used for various attacks

        // Shell escape commands
        blacklisted_commands_.insert("sh");
        blacklisted_commands_.insert("bash");
        blacklisted_commands_.insert("zsh");
        blacklisted_commands_.insert("fish");
        blacklisted_commands_.insert("dash");
        blacklisted_commands_.insert("tcsh");
        blacklisted_commands_.insert("csh");
        blacklisted_commands_.insert("ksh");

        // Editors that could modify system files
        blacklisted_commands_.insert("vim");
        blacklisted_commands_.insert("vi");
        blacklisted_commands_.insert("nano");
        blacklisted_commands_.insert("emacs");
        blacklisted_commands_.insert("ed");

        // Download tools (could download malware)
        blacklisted_commands_.insert("wget");
        blacklisted_commands_.insert("curl");
        blacklisted_commands_.insert("aria2c");
        blacklisted_commands_.insert("nc"); // netcat

        // Dangerous shell built-ins and patterns
        dangerous_patterns_.push_back("|");          // Pipe
        dangerous_patterns_.push_back(">");          // Redirect output
        dangerous_patterns_.push_back(">>");         // Append output
        dangerous_patterns_.push_back("<");          // Redirect input
        dangerous_patterns_.push_back("&");          // Background command
        dangerous_patterns_.push_back(";");          // Command separator
        dangerous_patterns_.push_back("$(");         // Command substitution
        dangerous_patterns_.push_back("`");          // Backtick substitution
        dangerous_patterns_.push_back("${");         // Variable expansion
        dangerous_patterns_.push_back("&&");         // AND operator
        dangerous_patterns_.push_back("||");         // OR operator
        dangerous_patterns_.push_back("\\n");        // Newline injection
        dangerous_patterns_.push_back("\\r");        // Carriage return injection
    }

    std::set<std::string> blacklisted_commands_;
    std::vector<std::string> dangerous_patterns_;
};
