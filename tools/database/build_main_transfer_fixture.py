#!/usr/bin/env python3
"""Independent synthetic source-layout fixture for the complete Map transfer.

Manually traced from TMapSvr/SSSender.cpp:1999–2638 and TItem.cpp:468.
No C++ codec/schema is imported. These are synthetic values, never backup rows.
Run from any directory; --check verifies the committed fixture without writing.
"""
import argparse
import json
from pathlib import Path
import struct


def build():
    data = bytearray()
    fields = {}

    def put(name, fmt, value):
        fields[name] = {'offset': len(data), 'format': fmt, 'value': value}
        data.extend(struct.pack('<' + fmt, value))

    def string(name, value):
        put(name + '.length', 'i', len(value))
        fields[name] = {'offset': len(data), 'format': 'raw', 'hex': value.hex()}
        data.extend(value)

    put('db_load', 'B', 0)
    put('char_id', 'I', 0xf0123456)
    put('key', 'I', 0x87654321)
    put('result', 'B', 0)
    string('name', b'H\xe9ro')
    for i, name in enumerate(('start_act', 'real_sex', 'class', 'level', 'race',
                              'country', 'original_country', 'sex', 'hair', 'face',
                              'body', 'pants', 'hand', 'foot', 'helmet_hide')):
        put(name, 'B', i + 1)
    for i, name in enumerate(('gold', 'silver', 'copper', 'experience', 'hp', 'mp')):
        put(name, 'I', 0x80000001 + i)
    put('skill_points', 'H', 0xabcd)
    put('region', 'I', 0xf0000001)
    put('guild_leave', 'B', 1)
    put('guild_leave_time', 'I', 0xdeadbeef)
    for i, name in enumerate(('map', 'spawn', 'last_spawn')):
        put(name, 'H', 50000 + i)
    put('last_destination', 'I', 0xcafebabe)
    put('tempted_monster', 'H', 65000)
    put('aftermath', 'B', 3)
    for name, value in [('x', 1.25), ('y', -2.5), ('z', 60000.75)]:
        put(name, 'f', value)
    put('direction', 'H', 32769)
    put('stat_level', 'B', 29)
    put('stat_point', 'B', 30)
    put('stat_exp', 'I', 123456789)
    put('save_age', 'I', 0xfedcba98)
    put('local_id', 'H', 0xaabb)
    put('login', 'B', 0)
    put('new_security', 'B', 0)
    string('security_code', b'ABC123')
    put('security_tries', 'B', 4)
    put('security_tick', 'I', 0x87650001)
    put('security_unlocked', 'B', 1)
    put('aid_country', 'B', 3)
    put('aid_date', 'q', -1)
    put('pc_bang', 'B', 1)
    put('pc_bang_time', 'I', 1234567)
    put('pc_bang_items', 'B', 8)
    put('lucky', 'B', 9)
    put('post_total', 'H', 400)
    put('post_read', 'H', 123)

    put('bags.count', 'H', 2)
    for i, bag in enumerate((1, 254)):
        put(f'bag{i}.id', 'B', bag)
        put(f'bag{i}.item', 'H', 40000 + i)
        put(f'bag{i}.expires', 'q', 1800000000 + i)
        put(f'bag{i}.eld', 'B', 1)
    put('cabinets.count', 'H', 1)
    put('cabinet.id', 'B', 7)
    put('cabinet.use', 'B', 1)
    put('items.count', 'H', 2)
    for i in range(2):
        p = f'item{i}.'
        for name, fmt, value in [('storage', 'B', i), ('storage_id', 'I', 1 if i == 0 else 7),
                                ('owner_type', 'B', 0), ('owner_id', 'I', 0xf0123456),
                                ('id', 'Q', 0xfedcba9876543210 + i), ('slot', 'B', 4 + i),
                                ('template', 'H', 0xbeef), ('level', 'B', 80), ('gem', 'B', 3),
                                ('appearance', 'H', 0xabcd), ('companion', 'I', 0x99887766),
                                ('count', 'B', 9), ('grade', 'B', 250), ('max_dura', 'I', 123000),
                                ('dura', 'I', 65432), ('refine', 'B', 7), ('expires', 'q', -2),
                                ('grade_effect', 'B', 11), ('eld', 'I', 0x11223344),
                                ('wrap', 'I', 0x22334455), ('color', 'I', 0x33445566),
                                ('guild', 'I', 0x44556677), ('texture', 'I', 0x55667788)]:
            put(p + name, fmt, value)
        put(p + 'magic.count', 'B', 2)
        for j, (mid, value) in enumerate(((50, 65432), (51, 4321))):
            put(p + f'magic{j}.id', 'B', mid)
            put(p + f'magic{j}.raw', 'H', value)
    put('skills.count', 'H', 1)
    put('skill.level', 'B', 7)
    put('skill.id', 'H', 40000)
    put('skill.remaining', 'I', 0xf0000002)

    def buff(p):
        for name, fmt, value in [('level', 'B', 4), ('id', 'H', 50001), ('remaining', 'I', 0xffffffff),
                                ('attack_type', 'B', 1), ('attack', 'I', 0xabcdef00),
                                ('host_type', 'B', 2), ('host', 'I', 0xbcdef001), ('country', 'B', 3)]:
            put(p + name, fmt, value)

    put('buffs.count', 'H', 1)
    buff('buff.')
    put('quests.count', 'H', 1)
    for name, fmt, value in [('id', 'I', 123456), ('remaining', 'I', 0x80001234),
                            ('completed', 'B', 1), ('triggered', 'B', 2), ('save', 'B', 1)]:
        put('quest.' + name, fmt, value)
    put('quest_terms.count', 'H', 1)
    for name, fmt, value in [('quest', 'I', 123456), ('id', 'I', 87654), ('type', 'B', 7), ('count', 'B', 8)]:
        put('term.' + name, fmt, value)
    put('hotkeys.count', 'H', 1)
    put('hotkey.inventory', 'B', 2)
    put('hotkey.save', 'B', 1)
    for i in range(12):
        put(f'hotkey{i}.type', 'B', i)
        put(f'hotkey{i}.id', 'H', 60000 + i)
    put('item_cooldowns.count', 'H', 1)
    put('item_cooldown.id', 'H', 50000)
    put('item_cooldown.remaining', 'I', 87654321)
    put('saddle.item', 'I', 0xaabbccdd)  # DWORD despite legacy member name m_wItemID
    put('saddle.expires', 'q', -3)
    put('saddle.type', 'B', 2)
    put('pets.count', 'H', 1)
    put('pet.id', 'H', 256)
    string('pet.name', b'Pet\x80')
    put('pet.time', 'q', 1800000123)
    put('pet.effect', 'B', 4)
    put('during_items.count', 'H', 1)
    for name, fmt, value in [('item', 'H', 34567), ('type', 'B', 2), ('remaining', 'I', 900000), ('expires', 'q', 1800000456)]:
        put('during.' + name, fmt, value)
    put('recalls.count', 'H', 1)
    for name, fmt, value in [('id', 'I', 0xf0f0f0f0), ('monster', 'H', 54321), ('pet', 'H', 400),
                            ('attribute', 'I', 0xff00ff00), ('level', 'B', 90), ('hp', 'I', 98765),
                            ('mp', 'I', 87654), ('skill_level', 'B', 6), ('x', 'H', 61000),
                            ('y', 'H', 62000), ('z', 'H', 63000), ('time', 'I', 456789), ('effect', 'B', 9)]:
        put('recall.' + name, fmt, value)
    put('recall_buffs.count', 'H', 1)
    put('recall_buff.id', 'I', 0xf0f0f0f0)
    buff('recall_buff.buff.')
    put('protected.count', 'H', 1)
    put('protected.id', 'I', 0xa0a0a0a0)
    string('protected.name', b'Other')
    put('protected.option', 'B', 3)
    put('protected.changed', 'B', 1)
    for name, fmt, value in [('available', 'I', 10), ('total', 'I', 20), ('rank', 'I', 30), ('percent', 'B', 40)]:
        put('pvp.' + name, fmt, value)
    for i in range(12):
        put(f'pvp.record{i}', 'I', 0x80000100 + i)

    def record(p):
        string(p + 'name', b'Opponent')
        for name, fmt, value in [('class', 'B', 5), ('level', 'B', 77), ('win', 'B', 1), ('points', 'I', 987654), ('time', 'q', -4)]:
            put(p + name, fmt, value)

    put('pvp_recent.count', 'H', 1)
    record('pvp_recent.')
    put('duel_records.count', 'H', 1)
    record('duel_record.')
    put('duel_sets', 'H', 1)
    for i in range(12):
        put(f'duel.score{i}', 'I', 0x90000100 + i)
    for i, name in enumerate(('auction_bids', 'auction_interests', 'auction_registrations')):
        put(name + '.count', 'H', 2)
        put(name + '.first', 'I', 0xa0000000 + i)
        put(name + '.second', 'I', 0xb0000000 + i)
    for name, fmt, value in [('points', 'I', 999999), ('wins', 'H', 1000), ('losses', 'H', 2000),
                            ('rank', 'I', 300000), ('percent', 'B', 33)]:
        put('month.' + name, fmt, value)
    put('titles.count', 'H', 1)
    put('title.id', 'H', 55555)
    put('title.selected', 'B', 1)
    put('companions.count', 'H', 1)
    for name, fmt, value in [('slot', 'B', 2), ('monster', 'I', 0xe0000001), ('exp', 'I', 12345),
                            ('next_exp', 'I', 23456), ('life', 'H', 45678), ('skill_points', 'B', 5), ('level', 'B', 99)]:
        put('companion.' + name, fmt, value)
    string('companion.name', b'Companion')
    put('companion.effect', 'B', 6)
    for i in range(6):
        put(f'companion.attribute{i}', 'H', 60000 + i)
    put('companion.bonus', 'B', 8)
    put('companion_items.count', 'H', 1)
    put('companion_items.slot', 'B', 2)
    put('companion_items.tick', 'I', 0xd0000001)
    for i in range(2):
        put(f'companion_items.item{i}', 'H', 50000 + i)
        put(f'companion_items.expires{i}', 'q', 1800001000 + i)
    for name, fmt, value in [('companion_slots', 'B', 3), ('medals', 'I', 0xf1f2f3f4),
                            ('rank_points', 'I', 0xa1a2a3a4), ('play_time', 'I', 0xb1b2b3b4)]:
        put(name, fmt, value)
    return bytes(data), fields


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    body, fields = build()
    directory = Path(__file__).resolve().parents[2] / 'Server/TMapSvrAsio/tests/fixtures'
    artifacts = {'main-transfer.hex': body.hex() + '\n',
                 'main-transfer-offsets.json': json.dumps({'synthetic': True, 'bytes': len(body), 'fields': fields}, indent=2) + '\n'}
    for name, text in artifacts.items():
        p = directory / name
        if args.check:
            assert p.read_text() == text, name
        else:
            directory.mkdir(parents=True, exist_ok=True)
            p.write_text(text)
    print(f'Independent synthetic transfer fixture: {len(body)} bytes, {len(fields)} named fields')
