# agents — Real Agent Implementations

## desktop-agent
DBus/xdg-open ilə tətbiq açma/bağlama, notification göndərmə.
Capabilities: `open_application`, `close_application`, `notifications`

## package-agent
APT/Pacman/Flatpak dəstəyi ilə paket idarəetməsi.
Capabilities: `install_package`, `remove_package`, `search_package`

## network-agent
NetworkManager (nmcli) ilə WiFi skan/bağlan/kəs.
Capabilities: `network_management`

Hər agent `lib/agent_client.h` kitabxanası ilə orchestrator-a qoşulur.
