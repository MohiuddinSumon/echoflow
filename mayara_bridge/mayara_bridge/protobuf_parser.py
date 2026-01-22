"""
Simple protobuf parser for RadarMessage.
This is a manual implementation - for production use, compile the .proto file.
"""

import struct
from typing import List, Dict, Optional


def decode_varint(data: bytes, offset: int) -> tuple[int, int]:
    """Decode a protobuf varint."""
    result = 0
    shift = 0
    pos = offset
    while pos < len(data):
        byte = data[pos]
        result |= (byte & 0x7F) << shift
        pos += 1
        if (byte & 0x80) == 0:
            break
        shift += 7
    return result, pos


def decode_length_delimited(data: bytes, offset: int) -> tuple[bytes, int]:
    """Decode a length-delimited field."""
    length, pos = decode_varint(data, offset)
    end = pos + length
    return data[pos:end], end


def parse_radar_message(data: bytes) -> Dict:
    """
    Parse a RadarMessage protobuf.
    
    Returns a dictionary with:
    - radar: radar ID
    - spokes: list of spoke dictionaries
    """
    result = {'radar': 0, 'spokes': []}
    pos = 0
    
    while pos < len(data):
        if pos >= len(data):
            break
        
        # Read field tag (wire type + field number)
        tag, pos = decode_varint(data, pos)
        if pos >= len(data):
            break
        
        field_number = tag >> 3
        wire_type = tag & 0x7
        
        if field_number == 1:  # radar (uint32)
            if wire_type == 0:  # varint
                result['radar'], pos = decode_varint(data, pos)
        
        elif field_number == 2:  # spokes (repeated Spoke)
            if wire_type == 2:  # length-delimited
                spoke_data, pos = decode_length_delimited(data, pos)
                spoke = parse_spoke(spoke_data)
                if spoke:
                    result['spokes'].append(spoke)
        else:
            # Skip unknown fields
            if wire_type == 0:  # varint
                _, pos = decode_varint(data, pos)
            elif wire_type == 2:  # length-delimited
                _, pos = decode_length_delimited(data, pos)
            elif wire_type == 1:  # 64-bit
                pos += 8
            elif wire_type == 5:  # 32-bit
                pos += 4
    
    return result


def parse_spoke(data: bytes) -> Optional[Dict]:
    """Parse a Spoke message."""
    spoke = {}
    pos = 0
    
    while pos < len(data):
        tag, pos = decode_varint(data, pos)
        if pos >= len(data):
            break
        
        field_number = tag >> 3
        wire_type = tag & 0x7
        
        if field_number == 1:  # angle (uint32)
            if wire_type == 0:
                spoke['angle'], pos = decode_varint(data, pos)
        
        elif field_number == 2:  # bearing (optional uint32)
            if wire_type == 0:
                spoke['bearing'], pos = decode_varint(data, pos)
        
        elif field_number == 3:  # range (uint32)
            if wire_type == 0:
                spoke['range'], pos = decode_varint(data, pos)
        
        elif field_number == 4:  # time (optional uint64)
            if wire_type == 0:
                spoke['time'], pos = decode_varint(data, pos)
        
        elif field_number == 5:  # data (bytes)
            if wire_type == 2:
                spoke['data'], pos = decode_length_delimited(data, pos)
        
        elif field_number == 6:  # lat (optional int64)
            if wire_type == 0:
                val, pos = decode_varint(data, pos)
                # Zigzag decode for signed int64
                spoke['lat'] = (val >> 1) ^ (-(val & 1))
        
        elif field_number == 7:  # lon (optional int64)
            if wire_type == 0:
                val, pos = decode_varint(data, pos)
                # Zigzag decode for signed int64
                spoke['lon'] = (val >> 1) ^ (-(val & 1))
        else:
            # Skip unknown fields
            if wire_type == 0:
                _, pos = decode_varint(data, pos)
            elif wire_type == 2:
                _, pos = decode_length_delimited(data, pos)
            elif wire_type == 1:
                pos += 8
            elif wire_type == 5:
                pos += 4
    
    return spoke if spoke else None
