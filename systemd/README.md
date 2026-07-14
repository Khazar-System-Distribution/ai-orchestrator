# systemd — System Integration

## Install
```bash
sudo cp ai-orchestrator.service ai-orchestrator.socket /etc/systemd/system/
sudo cp ai-orchestrator-agent@.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now ai-orchestrator.socket
sudo systemctl enable --now ai-orchestrator.service
```

## Agents
```bash
sudo systemctl enable --now ai-orchestrator-agent@desktop-agent
sudo systemctl enable --now ai-orchestrator-agent@package-agent
sudo systemctl enable --now ai-orchestrator-agent@network-agent
```
