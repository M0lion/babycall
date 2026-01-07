#!/usr/bin/env python3
"""
BLE Audio Stream Test Client

This script connects to the ESP32-C3 Baby Monitor BLE service and tests
all characteristics including audio streaming, control commands, and
device information.

Requirements:
    pip install bleak

Usage:
    python test_ble_client.py
"""

import asyncio
import struct
import sys
from bleak import BleakScanner, BleakClient
from typing import Optional

# Service and Characteristic UUIDs (128-bit format)
SERVICE_UUID = "00001900-0000-1000-8000-00805F9B34FB"
AUDIO_DATA_UUID = "00001901-0000-1000-8000-00805F9B34FB"
AUDIO_CONFIG_UUID = "00001902-0000-1000-8000-00805F9B34FB"
CONTROL_CMD_UUID = "00001903-0000-1000-8000-00805F9B34FB"
DEVICE_INFO_UUID = "00001904-0000-1000-8000-00805F9B34FB"

# Control command IDs
CMD_START_STREAM = 0x01
CMD_STOP_STREAM = 0x02
CMD_SET_GAIN = 0x04


class BabyMonitorClient:
    def __init__(self, device_name_prefix: str = "BabyMonitor-"):
        self.device_name_prefix = device_name_prefix
        self.client: Optional[BleakClient] = None
        self.audio_frame_count = 0
        self.control_response_count = 0

    async def find_device(self) -> Optional[str]:
        """Scan for and find the Baby Monitor device."""
        print(f"Scanning for devices with name starting with '{self.device_name_prefix}'...")
        devices = await BleakScanner.discover(timeout=10.0)

        for device in devices:
            if device.name and device.name.startswith(self.device_name_prefix):
                print(f"Found device: {device.name} ({device.address})")
                return device.address

        return None

    def audio_data_callback(self, sender, data: bytearray):
        """Callback for audio data notifications."""
        self.audio_frame_count += 1
        if self.audio_frame_count % 100 == 0:
            print(f"Received {self.audio_frame_count} audio frames (latest size: {len(data)} bytes)")

    def control_response_callback(self, sender, data: bytearray):
        """Callback for control command response notifications."""
        self.control_response_count += 1
        if len(data) >= 2:
            cmd_id = data[0]
            status = data[1]
            print(f"Control response: cmd_id=0x{cmd_id:02X}, status={status} ({'Success' if status == 0 else 'Error'})")

    async def connect(self, address: str) -> bool:
        """Connect to the device."""
        print(f"Connecting to {address}...")
        self.client = BleakClient(address)

        try:
            await self.client.connect()
            print(f"Connected! MTU: {self.client.mtu_size}")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False

    async def read_device_info(self):
        """Read and display device information."""
        print("\n--- Reading Device Info ---")
        try:
            data = await self.client.read_gatt_char(DEVICE_INFO_UUID)
            if len(data) >= 9:
                fw_version, battery_level, uptime = struct.unpack("<IBI", data)
                fw_major = (fw_version >> 16) & 0xFF
                fw_minor = (fw_version >> 8) & 0xFF
                fw_patch = fw_version & 0xFF
                print(f"Firmware Version: v{fw_major}.{fw_minor}.{fw_patch} (0x{fw_version:08X})")
                print(f"Battery Level: {battery_level}%")
                print(f"Uptime: {uptime} seconds ({uptime // 3600}h {(uptime % 3600) // 60}m)")
            else:
                print(f"Invalid device info length: {len(data)}")
        except Exception as e:
            print(f"Failed to read device info: {e}")

    async def read_audio_config(self):
        """Read and display audio configuration."""
        print("\n--- Reading Audio Config ---")
        try:
            data = await self.client.read_gatt_char(AUDIO_CONFIG_UUID)
            if len(data) >= 5:
                sample_rate, bit_depth, channels, status = struct.unpack("<HBBB", data)
                print(f"Sample Rate: {sample_rate} Hz")
                print(f"Bit Depth: {bit_depth} bits")
                print(f"Channels: {channels} ({'mono' if channels == 1 else 'stereo'})")
                print(f"Status: {status} ({'streaming' if status == 1 else 'stopped'})")
            else:
                print(f"Invalid audio config length: {len(data)}")
        except Exception as e:
            print(f"Failed to read audio config: {e}")

    async def write_audio_config(self, sample_rate: int, bit_depth: int, channels: int, status: int):
        """Write audio configuration."""
        print(f"\n--- Writing Audio Config ---")
        print(f"Sample Rate: {sample_rate} Hz, Bit Depth: {bit_depth}, Channels: {channels}, Status: {status}")
        try:
            data = struct.pack("<HBBB", sample_rate, bit_depth, channels, status)
            await self.client.write_gatt_char(AUDIO_CONFIG_UUID, data)
            print("Audio config written successfully")
        except Exception as e:
            print(f"Failed to write audio config: {e}")

    async def send_control_command(self, cmd_id: int, params: bytes = b""):
        """Send a control command."""
        cmd_name = {
            CMD_START_STREAM: "START_STREAM",
            CMD_STOP_STREAM: "STOP_STREAM",
            CMD_SET_GAIN: "SET_GAIN"
        }.get(cmd_id, f"UNKNOWN(0x{cmd_id:02X})")

        print(f"\n--- Sending Control Command: {cmd_name} ---")
        try:
            data = struct.pack("BB", cmd_id, len(params)) + params
            await self.client.write_gatt_char(CONTROL_CMD_UUID, data)
            print(f"Command sent successfully")
            await asyncio.sleep(0.5)  # Wait for response
        except Exception as e:
            print(f"Failed to send control command: {e}")

    async def enable_notifications(self):
        """Enable notifications for audio data and control responses."""
        print("\n--- Enabling Notifications ---")
        try:
            await self.client.start_notify(AUDIO_DATA_UUID, self.audio_data_callback)
            print("Audio data notifications enabled")
        except Exception as e:
            print(f"Failed to enable audio data notifications: {e}")

        try:
            await self.client.start_notify(CONTROL_CMD_UUID, self.control_response_callback)
            print("Control command notifications enabled")
        except Exception as e:
            print(f"Failed to enable control notifications: {e}")

    async def disable_notifications(self):
        """Disable all notifications."""
        print("\n--- Disabling Notifications ---")
        try:
            await self.client.stop_notify(AUDIO_DATA_UUID)
            await self.client.stop_notify(CONTROL_CMD_UUID)
            print("Notifications disabled")
        except Exception as e:
            print(f"Failed to disable notifications: {e}")

    async def run_test_sequence(self):
        """Run a complete test sequence."""
        print("\n=== Starting BLE Baby Monitor Test Sequence ===\n")

        # Read device information
        await self.read_device_info()

        # Read initial audio configuration
        await self.read_audio_config()

        # Enable notifications
        await self.enable_notifications()

        # Test control commands
        await self.send_control_command(CMD_START_STREAM)
        await asyncio.sleep(1)

        await self.send_control_command(CMD_SET_GAIN, struct.pack("B", 75))
        await asyncio.sleep(1)

        # Stream audio for 10 seconds
        print("\n--- Streaming Audio (10 seconds) ---")
        await asyncio.sleep(10)
        print(f"Total audio frames received: {self.audio_frame_count}")

        # Stop streaming
        await self.send_control_command(CMD_STOP_STREAM)
        await asyncio.sleep(1)

        # Change audio configuration
        await self.write_audio_config(sample_rate=8000, bit_depth=16, channels=1, status=0)
        await asyncio.sleep(0.5)

        # Read updated configuration
        await self.read_audio_config()

        # Disable notifications
        await self.disable_notifications()

        print("\n=== Test Sequence Complete ===\n")
        print(f"Total audio frames received: {self.audio_frame_count}")
        print(f"Total control responses received: {self.control_response_count}")

    async def disconnect(self):
        """Disconnect from the device."""
        if self.client and self.client.is_connected:
            print("\nDisconnecting...")
            await self.client.disconnect()
            print("Disconnected")


async def main():
    client = BabyMonitorClient()

    # Find device
    address = await client.find_device()
    if not address:
        print("Device not found. Make sure the ESP32-C3 is advertising.")
        return 1

    # Connect
    if not await client.connect(address):
        return 1

    try:
        # Run test sequence
        await client.run_test_sequence()
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
    except Exception as e:
        print(f"\nError during test: {e}")
        import traceback
        traceback.print_exc()
    finally:
        # Disconnect
        await client.disconnect()

    return 0


if __name__ == "__main__":
    try:
        exit_code = asyncio.run(main())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\nExiting...")
        sys.exit(0)
