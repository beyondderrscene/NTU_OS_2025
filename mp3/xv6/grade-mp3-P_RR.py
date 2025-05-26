#!/usr/bin/env python3

import re
from gradelib import *

import os

os.system("make -s --no-print-directory clean")
r = Runner(save("xv6.out"))

@test(2, "task1 - P_RR")
def test_task1():
    r.run_qemu(shell_script([
        'task1 PRR'
    ]), make_args=["SCHEDPOLICY=THREAD_SCHEDULER_PRIORITY_RR"])
    expected = r"""dispatch thread#2 at 0: allocated_time=4
thread #2 is running
thread#2 finish at 4
dispatch thread#1 at 4: allocated_time=6
thread #1 is running
thread#1 finish at 10"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')
    

@test(2, "task2 - P_RR")
def test_task2():
    r.run_qemu(shell_script([
        'task2 PRR'
    ]), make_args=["SCHEDPOLICY=THREAD_SCHEDULER_PRIORITY_RR"])
    expected = r"""dispatch thread#2 at 0: allocated_time=6
thread #2 is running
thread#2 finish at 6
dispatch thread#1 at 6: allocated_time=8
thread #1 is running
thread#1 finish at 14
dispatch thread#3 at 14: allocated_time=4
thread #3 is running
thread#3 finish at 18"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')
    
@test(2, "task3 - P_RR")
def test_task3():
    r.run_qemu(shell_script([
        'task3 PRR'
    ]), make_args=["SCHEDPOLICY=THREAD_SCHEDULER_PRIORITY_RR"])
    expected = r"""dispatch thread#2 at 0: allocated_time=3
thread #2 is running
thread#2 finish at 3
dispatch thread#1 at 3: allocated_time=7
thread #1 is running
thread#1 finish at 10
dispatch thread#3 at 10: allocated_time=10
thread #3 is running
thread#3 finish at 20"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')
    

@test(2, "task4 - P_RR")
def test_task4():
    r.run_qemu(shell_script([
        'task4 PRR'
    ]), make_args=["SCHEDPOLICY=THREAD_SCHEDULER_PRIORITY_RR"])
    expected = r"""dispatch thread#1 at 0: allocated_time=2
thread #1 is running
dispatch thread#2 at 2: allocated_time=2
thread #2 is running
thread#2 finish at 4
dispatch thread#3 at 4: allocated_time=2
thread #3 is running
dispatch thread#4 at 6: allocated_time=1
thread #4 is running
thread#4 finish at 7
dispatch thread#1 at 7: allocated_time=2
thread#1 finish at 9
dispatch thread#3 at 9: allocated_time=4
thread#3 finish at 13"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')

@test(2, "task5 - P_RR")
def test_task5():
    r.run_qemu(shell_script([
        'task5 PRR'
    ]), make_args=["SCHEDPOLICY=THREAD_SCHEDULER_PRIORITY_RR"])
    expected = r"""dispatch thread#2 at 0: allocated_time=2
thread #2 is running
thread#2 finish at 2
dispatch thread#1 at 2: allocated_time=2
thread #1 is running
dispatch thread#3 at 4: allocated_time=2
thread #3 is running
dispatch thread#1 at 6: allocated_time=2
dispatch thread#3 at 8: allocated_time=2
dispatch thread#1 at 10: allocated_time=2
thread#1 finish at 12
dispatch thread#3 at 12: allocated_time=1
thread#3 finish at 13
dispatch thread#4 at 13: allocated_time=4
thread #4 is running
thread#4 finish at 17"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')
    


run_tests()
os.system("make -s --no-print-directory clean")

