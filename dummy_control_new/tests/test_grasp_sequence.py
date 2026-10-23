import unittest
from controller import Controller
from kinematics import fk
from reference_motion import plan_grasp
from grasp_sequence import GraspSequence

class GraspTests(unittest.TestCase):
    def make(self):
        sent=[];c=Controller(sent.append,lambda _:None);c.enabled=True
        c.joints=(0,-75,180,0,0,0);c.pose=fk(c.joints);c.pose_at=c.joints_at=0
        return c,sent,GraspSequence(c,plan_grasp((160,0,240),c.joints),1)

    def test_open_then_two_moves_then_close(self):
        c,s,seq=self.make()
        for i in range(200):
            t=i*.1;c.pose_at=c.joints_at=t
            seq.tick(t)
            if c.hand_queued:
                c.query=None;c.tick(t)
                c.receive('ok hand open' if c.pending[0]=='hand_open' else 'ok hand close',t+.01)
            if c.target is not None:
                q=c.joint_target
                c.query=('joints',t);c.receive('ok '+' '.join(map(str,q)),t+.02)
                c.query=('pose',t+.03);c.receive('ok '+' '.join(map(str,fk(q))),t+.04)
            if seq.done:break
        self.assertTrue(seq.done)
        self.assertEqual(len(s),4)
        self.assertEqual(s[0],'!HAND_O');self.assertEqual(s[-1],'!HAND_C')
        self.assertTrue(all(cmd.startswith('>') for cmd in s[1:3]))
        self.assertEqual(c.completed,2)

    def test_no_ack_no_motion(self):
        c,s,seq=self.make();seq.tick(0);c.tick(.1)
        seq.tick(.5);self.assertEqual(s,['!HAND_O'])
        c.tick(3)
        with self.assertRaises(ValueError):seq.tick(3)
        self.assertNotIn('!HAND_C',s)

    def test_stop_cancels_queued_close(self):
        c,s,seq=self.make();seq.phase='close'
        goal=seq.plan['steps'][-1];c.joints=goal['joints'];c.pose=goal['expected']
        seq.tick(0);self.assertEqual(c.hand_queued,'hand_close')
        c.stop(.1)
        with self.assertRaises(ValueError):seq.tick(.2)
        self.assertNotIn('!HAND_C',s)

    def test_off_target_cannot_close(self):
        c,s,seq=self.make();seq.phase='close'
        with self.assertRaises(ValueError):seq.tick(0)
        self.assertEqual(s,[])

if __name__=='__main__':unittest.main()
