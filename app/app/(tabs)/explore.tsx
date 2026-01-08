import React, { useState, useEffect, useRef } from 'react';
import {
	StyleSheet,
	View,
	ScrollView,
	TouchableOpacity,
	Animated,
} from 'react-native';
import Slider from '@react-native-community/slider';
import { ThemedText } from '@/components/themed-text';
import { ThemedView } from '@/components/themed-view';
import { Colors } from '@/constants/theme';
import { useColorScheme } from '@/hooks/use-color-scheme';
import { useBLE } from '@/src/context/BLEContext';
import { IconSymbol } from '@/components/ui/icon-symbol';

/**
 * Monitor Screen
 * Live audio streaming and monitoring interface
 */
export default function MonitorScreen() {
	const colorScheme = useColorScheme();
	const colors = Colors[colorScheme ?? 'light'];
	const {
		isConnected,
		connectedDevice,
		disconnect,
		led,
		temperature,
		toggleLed,
	} = useBLE();

	const [volume, setVolumeState] = useState(0.8);
	const audioLevelAnim = useRef(new Animated.Value(0)).current;

	// Handle disconnect
	const handleDisconnect = async () => {
		await disconnect();
	};

	// Format uptime
	const formatUptime = (seconds: number): string => {
		const hours = Math.floor(seconds / 3600);
		const minutes = Math.floor((seconds % 3600) / 60);
		const secs = seconds % 60;
		return `${hours}h ${minutes}m ${secs}s`;
	};

	// Get firmware version string
	const getFirmwareVersion = (version: number): string => {
		const major = (version >> 16) & 0xff;
		const minor = (version >> 8) & 0xff;
		const patch = version & 0xff;
		return `v${major}.${minor}.${patch}`;
	};

	// Not connected state
	if (!isConnected) {
		return (
			<ThemedView style={styles.container}>
				<View style={styles.emptyState}>
					<IconSymbol
						size={80}
						name="antenna.radiowaves.left.and.right.slash"
						color={colors.icon}
					/>
					<ThemedText type="title" style={styles.emptyStateTitle}>
						No Device Connected
					</ThemedText>
					<ThemedText style={styles.emptyStateDescription}>
						Please connect to a baby monitor device from the Devices tab to start monitoring.
					</ThemedText>
				</View>
			</ThemedView>
		);
	}

	return (
		<ThemedView style={styles.container}>
			<ScrollView style={styles.scrollView} contentContainerStyle={styles.scrollContent}>
				{/* Header */}
				<View style={styles.header}>
					<ThemedText type="title" style={styles.title}>
						Audio Monitor
					</ThemedText>
				</View>

				{/* Connection Status */}
				<View style={[styles.card, styles.connectionCard]}>
					<View style={styles.cardHeader}>
						<IconSymbol
							size={24}
							name="checkmark.circle.fill"
							color="#4CAF50"
						/>
						<ThemedText type="defaultSemiBold" style={styles.cardTitle}>
							Connected Device
						</ThemedText>
					</View>
					<ThemedText style={styles.deviceNameLarge}>
						{connectedDevice?.name || 'Unknown Device'}
					</ThemedText>
					<ThemedText style={styles.deviceIdSmall}>
						{connectedDevice?.id}
					</ThemedText>
				</View>

				{/* Streaming Controls */}
				<View style={[styles.card, { backgroundColor: colors.background }]}>
					<View style={styles.cardHeader}>
						<IconSymbol
							size={24}
							name="play.circle"
							color={colors.tint}
						/>
						<ThemedText type="defaultSemiBold" style={styles.cardTitle}>
							Streaming Control
						</ThemedText>
					</View>
					{!led ? (
						<TouchableOpacity
							style={[styles.streamButton, { backgroundColor: colors.tint }]}
							onPress={toggleLed}
						>
							<IconSymbol size={24} name="play.fill" color="#fff" />
							<ThemedText style={styles.streamButtonText}>
								Turn on led
							</ThemedText>
						</TouchableOpacity>
					) : (
						<TouchableOpacity
							style={[styles.streamButton, styles.stopButton]}
							onPress={toggleLed}
						>
							<IconSymbol size={24} name="stop.fill" color="#fff" />
							<ThemedText style={styles.streamButtonText}>
								Turn off led
							</ThemedText>
						</TouchableOpacity>
					)}
				</View>

				{/* Device Info */}
				<View style={[styles.card, { backgroundColor: colors.background }]}>
					<View style={styles.cardHeader}>
						<IconSymbol
							size={24}
							name="info.circle"
							color={colors.tint}
						/>
						<ThemedText type="defaultSemiBold" style={styles.cardTitle}>
							Temperature {temperature}
						</ThemedText>
					</View>
				</View>

				{/* Disconnect Button */}
				<TouchableOpacity
					style={[styles.disconnectButton, { borderColor: colors.tint }]}
					onPress={handleDisconnect}
				>
					<IconSymbol size={20} name="xmark.circle" color={colors.tint} />
					<ThemedText style={[styles.disconnectButtonText, { color: colors.tint }]}>
						Disconnect Device
					</ThemedText>
				</TouchableOpacity>
			</ScrollView>
		</ThemedView>
	);
}

const styles = StyleSheet.create({
	container: {
		flex: 1,
	},
	scrollView: {
		flex: 1,
	},
	scrollContent: {
		padding: 20,
	},
	header: {
		marginBottom: 24,
	},
	title: {
		marginBottom: 8,
	},
	subtitle: {
		opacity: 0.7,
	},
	card: {
		padding: 20,
		borderRadius: 12,
		marginBottom: 16,
		shadowColor: '#000',
		shadowOffset: { width: 0, height: 2 },
		shadowOpacity: 0.1,
		shadowRadius: 4,
		elevation: 3,
	},
	connectionCard: {
		backgroundColor: '#E8F5E9',
		borderWidth: 2,
		borderColor: '#4CAF50',
	},
	cardHeader: {
		flexDirection: 'row',
		alignItems: 'center',
		marginBottom: 16,
		gap: 8,
	},
	cardTitle: {
		fontSize: 16,
		flex: 1,
	},
	deviceNameLarge: {
		fontSize: 20,
		fontWeight: '600',
		marginBottom: 4,
	},
	deviceIdSmall: {
		fontSize: 12,
		opacity: 0.6,
	},
	audioVisualizerContainer: {
		gap: 12,
	},
	audioLevelBarBackground: {
		height: 40,
		borderRadius: 8,
		overflow: 'hidden',
	},
	audioLevelBar: {
		height: '100%',
		borderRadius: 8,
	},
	audioLevelText: {
		textAlign: 'center',
		fontSize: 14,
		opacity: 0.7,
	},
	streamButton: {
		flexDirection: 'row',
		alignItems: 'center',
		justifyContent: 'center',
		paddingVertical: 16,
		paddingHorizontal: 24,
		borderRadius: 12,
		gap: 12,
	},
	stopButton: {
		backgroundColor: '#F44336',
	},
	streamButtonText: {
		color: '#fff',
		fontWeight: '600',
		fontSize: 16,
	},
	volumeControl: {
		flexDirection: 'row',
		alignItems: 'center',
		gap: 12,
		marginBottom: 8,
	},
	volumeSlider: {
		flex: 1,
		height: 40,
	},
	volumeText: {
		textAlign: 'center',
		fontSize: 14,
		opacity: 0.7,
	},
	statsGrid: {
		flexDirection: 'row',
		flexWrap: 'wrap',
		gap: 16,
	},
	statItem: {
		flex: 1,
		minWidth: '45%',
	},
	statLabel: {
		fontSize: 12,
		opacity: 0.6,
		marginBottom: 4,
	},
	statValue: {
		fontSize: 18,
	},
	refreshButton: {
		padding: 4,
	},
	disconnectButton: {
		flexDirection: 'row',
		alignItems: 'center',
		justifyContent: 'center',
		paddingVertical: 16,
		paddingHorizontal: 24,
		borderRadius: 12,
		borderWidth: 2,
		marginTop: 8,
		marginBottom: 20,
		gap: 8,
	},
	disconnectButtonText: {
		fontWeight: '600',
		fontSize: 16,
	},
	emptyState: {
		flex: 1,
		alignItems: 'center',
		justifyContent: 'center',
		padding: 40,
	},
	emptyStateTitle: {
		marginTop: 24,
		marginBottom: 12,
		textAlign: 'center',
	},
	emptyStateDescription: {
		textAlign: 'center',
		opacity: 0.7,
		fontSize: 16,
	},
});
