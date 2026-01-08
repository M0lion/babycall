import { Device } from 'react-native-ble-plx';

/**
 * Audio configuration matching ESP32 binary format
 */
export interface AudioConfig {
	sampleRate: number;    // uint16_t (2 bytes)
	bitDepth: number;      // uint8_t (1 byte)
	channels: number;      // uint8_t (1 byte) - 1=mono
	status: number;        // uint8_t (1 byte) - 0=stopped, 1=streaming
}

/**
 * Device information matching ESP32 binary format
 */
export interface DeviceInfo {
	firmwareVersion: number;  // uint32_t (4 bytes) - e.g., 0x00010000 for v1.0.0
	batteryLevel: number;     // uint8_t (1 byte) - 0-100
	uptime: number;           // uint32_t (4 bytes) - seconds
}

/**
 * Control commands matching ESP32 protocol
 */
export enum ControlCommand {
	START_STREAM = 0x01,
	STOP_STREAM = 0x02,
	SET_GAIN = 0x04,
}

/**
 * BLE Service UUIDs (128-bit format)
 */
export const BLE_UUIDS = {
	// Service UUID
	SERVICE: 'f0debc9a-7856-3412-f0de-bc9a78563412',

	// Characteristic UUIDs  
	TEMPERATURE: '0100bc9a-7856-3412-f0de-bc9a78563412',  // Read + Notify
	LED: '0200bc9a-7856-3412-f0de-bc9a78563412',          // Read + Write
};

/**
 * BLE MTU size for negotiation
 */
export const BLE_MTU_SIZE = 512;

/**
 * Re-export Device type from react-native-ble-plx for convenience
 */
export type { Device };
