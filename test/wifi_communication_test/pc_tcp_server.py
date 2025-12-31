#!/usr/bin/env python3
"""
TCP Server for ESP32/ESP8266 Communication Test
This script creates a TCP server that accepts connections from ESP devices
and allows bidirectional communication.
"""

import socket
import threading
import time
import argparse
import sys

# Default settings
DEFAULT_HOST = '0.0.0.0'  # Listen on all available interfaces
DEFAULT_PORT = 8888

class ArduinoTCPServer:
    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT):
        self.host = host
        self.port = port
        self.server_socket = None
        self.running = False
        self.clients = []
        self.client_lock = threading.Lock()
        
    def start(self):
        """Start the TCP server"""
        try:
            # Create socket
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            
            # Bind to address and port
            self.server_socket.bind((self.host, self.port))
            
            # Listen for connections
            self.server_socket.listen(5)
            self.running = True
            
            print(f"Server started on {self.host}:{self.port}")
            print("Waiting for connections...")
            
            # Get local IP for convenience
            local_ip = self.get_local_ip()
            if local_ip:
                print(f"Your local IP address is: {local_ip}")
                print(f"Make sure to set serverIP = \"{local_ip}\" in your ESP code")
            
            # Start accept thread
            accept_thread = threading.Thread(target=self.accept_connections)
            accept_thread.daemon = True
            accept_thread.start()
            
            # Start command thread
            command_thread = threading.Thread(target=self.command_loop)
            command_thread.daemon = True
            command_thread.start()
            
            # Keep main thread alive
            while self.running:
                time.sleep(1)
                
        except KeyboardInterrupt:
            print("\nShutting down server...")
        except Exception as e:
            print(f"Error: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """Stop the server and close all connections"""
        self.running = False
        
        # Close all client connections
        with self.client_lock:
            for client, addr, _ in self.clients:
                try:
                    client.close()
                except:
                    pass
            self.clients = []
        
        # Close server socket
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
    
    def accept_connections(self):
        """Accept incoming connections"""
        while self.running:
            try:
                client_socket, client_address = self.server_socket.accept()
                print(f"New connection from {client_address[0]}:{client_address[1]}")
                
                # Create client thread
                client_thread = threading.Thread(
                    target=self.handle_client,
                    args=(client_socket, client_address)
                )
                client_thread.daemon = True
                client_thread.start()
                
                # Add to clients list
                with self.client_lock:
                    self.clients.append((client_socket, client_address, client_thread))
                    
            except Exception as e:
                if self.running:
                    print(f"Error accepting connection: {e}")
                break
    
    def handle_client(self, client_socket, client_address):
        """Handle communication with a client"""
        try:
            while self.running:
                data = client_socket.recv(1024)
                if not data:
                    break
                
                # Process received data
                message = data.decode('utf-8').strip()
                print(f"From {client_address[0]}: {message}")
                
                # You can add specific responses here if needed
                if message.startswith("SENSOR:"):
                    # Just log sensor data, no response needed
                    pass
                    
        except Exception as e:
            print(f"Error handling client {client_address[0]}: {e}")
        finally:
            # Close connection and remove from list
            client_socket.close()
            with self.client_lock:
                self.clients = [(c, a, t) for c, a, t in self.clients 
                               if a != client_address]
            print(f"Connection from {client_address[0]}:{client_address[1]} closed")
    
    def command_loop(self):
        """Handle user input for commands"""
        help_text = """
Available commands:
  list              - List all connected clients
  send <message>    - Send message to all clients
  led:on            - Turn on LED on all connected devices
  led:off           - Turn off LED on all connected devices
  get:sensor        - Request sensor readings from all devices
  quit              - Exit the server
  help              - Show this help message
"""
        print(help_text)
        
        while self.running:
            try:
                command = input("Command > ").strip()
                
                if command.lower() == "quit":
                    print("Shutting down server...")
                    self.running = False
                    break
                    
                elif command.lower() == "list":
                    with self.client_lock:
                        if not self.clients:
                            print("No clients connected")
                        else:
                            print("Connected clients:")
                            for i, (_, addr, _) in enumerate(self.clients):
                                print(f"  {i+1}. {addr[0]}:{addr[1]}")
                
                elif command.lower() == "help":
                    print(help_text)
                    
                elif command.lower() == "led:on":
                    self.send_to_all("LED:ON")
                    
                elif command.lower() == "led:off":
                    self.send_to_all("LED:OFF")
                    
                elif command.lower() == "get:sensor":
                    self.send_to_all("GET:SENSOR")
                    
                elif command.lower().startswith("send "):
                    message = command[5:]
                    self.send_to_all(message)
                    
                else:
                    print("Unknown command. Type 'help' for available commands.")
                    
            except Exception as e:
                print(f"Error in command loop: {e}")
    
    def send_to_all(self, message):
        """Send a message to all connected clients"""
        with self.client_lock:
            if not self.clients:
                print("No clients connected")
                return
                
            for client, addr, _ in self.clients:
                try:
                    client.sendall((message + "\n").encode('utf-8'))
                    print(f"Sent '{message}' to {addr[0]}:{addr[1]}")
                except Exception as e:
                    print(f"Error sending to {addr[0]}: {e}")
    
    @staticmethod
    def get_local_ip():
        """Get local IP address"""
        try:
            # Create a temporary socket to determine local IP
            temp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            temp_socket.connect(("8.8.8.8", 80))  # Doesn't actually send data
            local_ip = temp_socket.getsockname()[0]
            temp_socket.close()
            return local_ip
        except:
            return None

def main():
    """Main entry point with argument parsing"""
    parser = argparse.ArgumentParser(description='TCP Server for ESP32/ESP8266 communication')
    parser.add_argument('--host', default=DEFAULT_HOST, help=f'Host address to bind to (default: {DEFAULT_HOST})')
    parser.add_argument('--port', type=int, default=DEFAULT_PORT, help=f'Port to listen on (default: {DEFAULT_PORT})')
    
    args = parser.parse_args()
    
    server = ArduinoTCPServer(args.host, args.port)
    try:
        server.start()
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        server.stop()

if __name__ == "__main__":
    main()
