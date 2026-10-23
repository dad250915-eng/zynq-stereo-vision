import sys,unittest,queue,threading
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from controller import Controller
from serial_link import SerialLink
J='ok 0 -75 180 0 0 0'
P='ok 80.82 0 175.56 180 75 180'

class RecoveryTests(unittest.TestCase):
    def make(self,moving=False):
        s=[];l=[];c=Controller(s.append,l.append);c.enabled=True
        c.pose=(80.82,0,175.56,180,75,180);c.joints=(0,-75,180,0,0,0)
        c.pose_at=c.joints_at=0;c.query=('pose',0)
        if moving:c.target=(100,0,200,180,75,180);c.started=0;c.source_pose=c.pose
        return c,s,l

    def test_real_bad_line_recover_and_no_motion_replay(self):
        c,s,l=self.make(True);old=c.pose;target=c.target
        c.receive('ok 129.82 46.70 75.50 -148.64 -41.78 ��.��',.1)
        self.assertEqual(c.pose,old);self.assertEqual(c.target,target);self.assertTrue(c.enabled)
        self.assertIsNone(c.pose_at);self.assertIsNotNone(c.recovery)
        with self.assertRaises(ValueError):c.move([1,2,3],[0,0,0],5,.2)
        c.receive(P,1);self.assertEqual(c.pose,old)
        for t,reply in ((4,J),(7,J),(10,P),(13,P)):
            c.tick(t);c.receive(reply,t+.1)
        self.assertIsNone(c.recovery);self.assertTrue(c.await_fresh)
        self.assertEqual(s,['#GETJPOS','#GETJPOS','#GETLPOS','#GETLPOS'])
        self.assertEqual(c.arrivals,0);self.assertIsNone(c.fault)
        c.tick(16);c.receive(J,16.1);c.tick(16.4);c.receive(P,16.5)
        self.assertFalse(c.await_fresh);self.assertTrue(any('RECOVERY_OK' in x for x in l))

    def test_bad_formats_never_replace_pose(self):
        for line in ('ok 1 2 3 4 5', 'ok 1 2 3 4 5 nan','ok 1 2 3 102.1\x00 5 6'):
            c,s,l=self.make();old=c.pose;c.receive(line,.1)
            self.assertIsNotNone(c.recovery);self.assertEqual(c.pose,old);self.assertEqual(s,[])

    def test_raw_partial_bytes_extend_quiet_window(self):
        c,s,l=self.make();c.receive('ok bad',.1);c.raw_activity(2)
        c.tick(3);self.assertEqual(s,[]);c.tick(4.6);self.assertEqual(s,['#GETJPOS'])

    def test_swapped_probe_replies_do_not_recover(self):
        c,s,l=self.make();c.receive('ok bad',.1)
        for t,line in ((3,P),(6,P),(9,J),(12,J)):c.tick(t);c.receive(line,t+.1)
        self.assertIsNotNone(c.recovery);self.assertIsNone(c.pose_at)

    def test_recovery_deadline_still_stops_once_when_moving(self):
        c,s,l=self.make(True);c.receive('ok bad',.1);c.tick(31);c.tick(32)
        self.assertIsNotNone(c.fault);self.assertIsNone(c.enabled)
        self.assertEqual(s,['!STOP']);self.assertTrue(any(x.startswith('FAULT') for x in l))

    def test_stop_available_during_recovery(self):
        c,s,l=self.make(True);c.receive('ok bad',.1);c.stop(.2);c.receive('Stopped ok',.3)
        self.assertEqual(s,['!STOP']);self.assertIsNone(c.target)

    def test_reader_preserves_bytes_before_decode_and_handles_split(self):
        events=queue.Queue();link=SerialLink('TEST',events)
        class Fake:
            def __init__(self):self.chunks=[b'ok 1 2 ',b'3 4 5 \xff\x00\r\n']
            @property
            def in_waiting(self):return 1
            def read(self,n):
                if self.chunks:return self.chunks.pop(0)
                link.stop.set();return b''
        link._read(Fake(),0);items=[]
        while not events.empty():items.append(events.get())
        raw=[x[3] for x in items if x[2]=='raw']
        self.assertEqual(b''.join(raw),b'ok 1 2 3 4 5 \xff\x00\r\n')
        self.assertEqual([x[3] for x in items if x[2]=='line'],['ok 1 2 3 4 5 �\x00'])
