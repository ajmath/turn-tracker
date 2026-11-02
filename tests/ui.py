"""Exercise the UI's persistent native bridge using real gestures and replay."""
import json
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Bridge:
    def __init__(self):
        self.process = subprocess.Popen([str(ROOT / 'build/table-ui')], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, text=True)
        self.commands = []

    def send(self, command):
        self.commands.append(command)
        self.process.stdin.write(command + '\n')
        self.process.stdin.flush()
        return json.loads(self.process.stdout.readline())

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=3)
        self.process.stdout.close()


class UiBridgeTests(unittest.TestCase):
    def setUp(self):
        self.bridge = Bridge()

    def tearDown(self):
        self.bridge.close()

    def game(self):
        b = self.bridge
        b.send('reset 3 42')
        b.send('advance 100')
        b.send('gesture 0 enroll')
        state = b.send('advance 5000')
        self.assertEqual(state['pucks'][0]['status'], 2)
        self.assertEqual(state['pucks'][0]['selected'], [])
        for i in range(3):
            b.send(f'gesture {i} tap')
            b.send('advance 1000')
        self.assertEqual(b.send('state')['pucks'][0]['selected'], [0, 1, 2])
        b.send('gesture 0 hold')
        state = b.send('advance 10000')
        self.assertTrue(all(p['status'] == 6 for p in state['pucks']))
        self.assertEqual([p['led'] for p in state['pucks']], [True, False, False])
        return state

    def test_game_recovery_and_replay(self):
        b = self.bridge
        self.game()
        b.send('gesture 0 tap')
        state = b.send('advance 1000')
        self.assertTrue(all(p['active'] == 1 for p in state['pucks']))
        b.send('isolate 2 1')
        state = b.send('advance 5000')
        self.assertEqual(state['pucks'][0]['status'], 7)
        b.send('isolate 2 0')
        state = b.send('advance 5000')
        self.assertTrue(all(p['status'] == 6 for p in state['pucks']))
        b.send('power 1 0')
        b.send('advance 5000')
        b.send('power 1 1')
        final = b.send('advance 5000')
        self.assertTrue(all(p['status'] == 6 for p in final['pucks']))
        commands = list(b.commands)
        other = Bridge()
        try:
            for command in commands:
                replay = other.send(command)
            self.assertEqual(final, replay)
        finally:
            other.close()

    def test_inputs_faults_and_validation(self):
        b = self.bridge
        for invalid in ('reset 9 1', 'advance 11', 'advance 10001', 'button 100 1', 'link 0 0 1'):
            self.assertTrue(b.send(invalid)['error'])
        b.send('advance 100')
        b.send('button 0 1')
        state = b.send('advance 3100')
        self.assertTrue(state['pucks'][0]['down'])
        b.send('button 0 0')
        b.send('advance 100')
        for _ in range(2):
            b.send('gesture 0 tap')
            b.send('advance 200')
        b.send('storage 0 1')
        b.send('gesture 0 tap')
        state = b.send('advance 200')
        self.assertTrue(state['pucks'][0]['storageFault'])
        self.assertEqual(state['pucks'][0]['status'], 9)
        self.assertEqual(state['pucks'][0]['selected'], [])
        b.send('storage 0 0')
        b.send('gesture 0 tap')
        state = b.send('advance 200')
        self.assertEqual(state['pucks'][0]['selected'], [0])
        state = b.send('faults 1')
        self.assertTrue(state['faults'])
        state = b.send('link 1 0 1')
        self.assertTrue(state['links'][1][0])
        self.assertFalse(state['links'][0][1])


if __name__ == '__main__':
    unittest.main()
