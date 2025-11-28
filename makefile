run_all:
	gnome-terminal -- bash -c "python3 multiclient_server.py; exec bash"
	gnome-terminal -- bash -c "python3 radar_client_r1.py; exec bash"
	gnome-terminal -- bash -c "python3 radar_client_r2.py; exec bash"
.PHONY: run_all