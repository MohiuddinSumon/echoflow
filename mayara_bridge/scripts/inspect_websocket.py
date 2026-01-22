#!/usr/bin/env python3
"""
WebSocket message inspector for Mayara.
Connects to Mayara's WebSocket and displays incoming protobuf messages.
"""

import asyncio
import websockets
import sys
import json
from datetime import datetime


async def inspect_websocket(uri, verbose=False, max_messages=10):
    """Connect to WebSocket and display messages."""
    print(f"Connecting to: {uri}")
    print("=" * 60)
    
    try:
        async with websockets.connect(uri) as websocket:
            print("✓ Connected!")
            print("Waiting for messages...\n")
            
            message_count = 0
            
            async for message in websocket:
                message_count += 1
                
                # Parse protobuf (simplified - just show raw bytes info)
                if isinstance(message, bytes):
                    size = len(message)
                    print(f"\n[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] Message #{message_count}")
                    print(f"  Size: {size} bytes")
                    
                    if verbose:
                        # Show first 100 bytes as hex
                        hex_preview = message[:100].hex()
                        print(f"  First 100 bytes (hex): {hex_preview}")
                        
                        # Try to show as integers
                        if size > 0:
                            first_bytes = [b for b in message[:20]]
                            print(f"  First 20 bytes (int): {first_bytes}")
                else:
                    print(f"\n[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] Message #{message_count}")
                    print(f"  Type: {type(message)}")
                    print(f"  Content: {str(message)[:200]}")
                
                if max_messages > 0 and message_count >= max_messages:
                    print(f"\nReceived {max_messages} messages. Stopping.")
                    break
                    
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Inspect Mayara WebSocket messages')
    parser.add_argument('--host', default='localhost', help='Mayara host')
    parser.add_argument('--port', type=int, default=6502, help='Mayara port')
    parser.add_argument('--radar-id', default='radar-1', help='Radar ID')
    parser.add_argument('--verbose', '-v', action='store_true', help='Show detailed message content')
    parser.add_argument('--max-messages', type=int, default=10, help='Maximum messages to receive (0 = unlimited)')
    
    args = parser.parse_args()
    
    uri = f"ws://{args.host}:{args.port}/v1/api/spokes/{args.radar_id}"
    
    asyncio.run(inspect_websocket(uri, args.verbose, args.max_messages))


if __name__ == '__main__':
    main()
