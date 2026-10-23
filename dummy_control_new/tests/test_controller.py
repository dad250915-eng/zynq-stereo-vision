import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from controller import Controller


class ProtocolTests(unittest.TestCase):
    def make(self):
        sent=[];logs=[];return Controller(sent.append,logs.append),sent,logs

    def enabled(self):
        c,s,l=self.make();c.enable(0);c.receive('Started ok',.1)
        c.tick(.2);c.receive('ok 80.82 0 175.56 180 75 180',.3)
        c.tick(.6);c.receive('ok 0 -75 180 0 0 0',.7)
        return c,s,l

    def test_no_startup_motion_and_serialized_query_types(self):
        c,s,l=self.make();self.assertEqual(s,[]);c.tick(0)
        self.assertEqual(s,['#GETLPOS']);c.tick(.2);self.assertEqual(len(s),1)
        c.receive('ok 80 0 170 180 75 180',.3);c.tick(.6)
        self.assertEqual(s[-1],'#GETJPOS');c.receive('ok 0 -75 180 0 0 0',.7)
        self.assertEqual(c.pose[:3],(80,0,170));self.assertEqual(c.joints[1],-75)

    def test_official_command_format_no_range_validation(self):
        c,s,l=self.enabled();c.move([100,20,200],[180,75,180],5,1)
        self.assertEqual(s[-1],'@100.0,20.0,200.0,180.0,75.0,180.0,5')
        self.assertIn('参考限位',c.observation)
        with self.assertRaises(ValueError):c.move([110,20,200],[180,75,180],5,1.1)

    def test_queue_and_bare_ok_not_arrival(self):
        c,s,l=self.enabled();c.move([100,20,200],[180,75,180],5,1)
        c.receive('15ok',1.1);c.receive('ok',1.2)
        self.assertEqual(c.queue_space,15);self.assertEqual(c.completed,0);self.assertIsNotNone(c.target)

    def test_pose_arrival_ignores_joint_reply(self):
        c,s,l=self.enabled();c.move([100,20,200],[180,75,180],5,1)
        for t in (2,3,4):
            c.query=('joints',t);c.receive('ok 100 20 200 180 75 180',t+.1)
        self.assertEqual(c.completed,0)
        for t in (5,6,7):
            c.query=('pose',t);c.receive('ok 100 20 200 180 75 180',t+.1)
        self.assertEqual(c.completed,1);self.assertIsNone(c.target)

    def test_late_reply_fault_cannot_be_misclassified(self):
        c,s,l=self.make();c.tick(0);c.tick(2.1)
        self.assertIsNotNone(c.fault);c.receive('ok 1 2 3 4 5 6',2.2)
        self.assertIsNone(c.pose);self.assertIsNone(c.joints);self.assertEqual(s,['#GETLPOS'])

    def test_motion_query_fault_requests_one_stop(self):
        c,s,l=self.enabled();c.move([100,20,200],[180,75,180],5,1)
        c.tick(2);c.tick(4.1);c.tick(5)
        self.assertEqual(s.count('!STOP'),1);self.assertFalse(c.enabled)

    def test_slow_move_not_stopped_at_sixty_seven_seconds(self):
        c,s,l=self.enabled();c.move([100,20,200],[180,75,180],5,1)
        for t in range(2,70):
            c.tick(t)
            if c.query:
                kind,_=c.query
                c.receive('ok 80.82 0 175.56 180 75 180' if kind=='pose' else 'ok 0 -75 180 0 0 0',t+.1)
        self.assertNotIn('!STOP',s);self.assertIsNone(c.fault)
        self.assertTrue(any('67秒' in x for x in l));self.assertTrue(any('5秒' in x for x in l))

    def test_esp_boot_messages_prevent_commands(self):
        c,s,l=self.make();c.receive('flash read err, 1000',0)
        with self.assertRaises(ValueError):c.enable(1)
        self.assertEqual(s,[])

    def test_invalid_and_zero_motion_do_not_send(self):
        c,s,l=self.enabled();before=len(s)
        for p in ([float('nan'),0,0],[float('inf'),0,0]):
            with self.assertRaises(ValueError):c.move(p,[0,0,0],5,1)
        c.move([80.82,0,175.56],[180,75,180],5,1)
        self.assertEqual(len(s),before)

    def test_stop_ack_disables_and_clears_target(self):
        c,s,l=self.enabled();c.move([100,20,200],[180,75,180],5,1)
        c.stop(2);c.receive('Stopped ok',2.1)
        self.assertFalse(c.enabled);self.assertIsNone(c.target)


if __name__=='__main__':unittest.main()
