#!/usr/bin/env python3

import re
from gradelib import *

import os

os.system("make -s --no-print-directory clean")
r = Runner(save("xv6.out"))

@test(2, "task1")
def test_uthread():
    r.run_qemu(shell_script([
        'rttask1 EDF_CBS'
    ]), make_args = ["SCHEDPOLICY=THREAD_SCHEDULER_EDF_CBS"])
    expected = """dispatch thread#1 at 0: allocated_time=7
thread#1 finish one cycle at 7: 2 cycles left
run_queue is empty, sleep for 3 ticks
dispatch thread#1 at 10: allocated_time=7
thread#1 finish one cycle at 17: 1 cycles left
run_queue is empty, sleep for 3 ticks
dispatch thread#1 at 20: allocated_time=7
thread#1 finish one cycle at 27: 0 cycles left"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')

@test(2, "task2")
def test_uthread():
    r.run_qemu(shell_script([
        'rttask2 EDF_CBS'
    ]), make_args = ["SCHEDPOLICY=THREAD_SCHEDULER_EDF_CBS"])
    expected = """dispatch thread#1 at 0: allocated_time=3
dispatch thread#2 at 3: allocated_time=4
thread#2 finish one cycle at 7: 1 cycles left
dispatch thread#1 at 7: allocated_time=2
thread#1 finish one cycle at 9: 1 cycles left
dispatch thread#2 at 9: allocated_time=4
thread#2 finish one cycle at 13: 0 cycles left
dispatch thread#1 at 13: allocated_time=5
thread#1 finish one cycle at 18: 0 cycles left"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')

@test(2, "task3")
def test_uthread():
    r.run_qemu(shell_script([
        'rttask3 EDF_CBS'
    ]), make_args = ["SCHEDPOLICY=THREAD_SCHEDULER_EDF_CBS"])
    expected = """dispatch thread#1 at 0: allocated_time=15
thread#1 finish one cycle at 15: 1 cycles left
dispatch thread#2 at 15: allocated_time=10
thread#2 finish one cycle at 25: 1 cycles left
dispatch thread#1 at 25: allocated_time=15
thread#1 finish one cycle at 40: 0 cycles left
dispatch thread#2 at 40: allocated_time=10
thread#2 finish one cycle at 50: 0 cycles left"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')
    
@test(2, "task4")
def test_uthread():
    r.run_qemu(shell_script([
        'rttask4 EDF_CBS'
    ]), make_args = ["SCHEDPOLICY=THREAD_SCHEDULER_EDF_CBS"])
    expected = """dispatch thread#1 at 0: allocated_time=10
thread#1 finish one cycle at 10: 1 cycles left
dispatch thread#2 at 10: allocated_time=8
thread#2 finish one cycle at 18: 1 cycles left
dispatch thread#3 at 18: allocated_time=2
dispatch thread#2 at 20: allocated_time=8
thread#2 finish one cycle at 28: 0 cycles left
dispatch thread#1 at 28: allocated_time=10
thread#1 finish one cycle at 38: 0 cycles left
dispatch thread#3 at 38: allocated_time=13
thread#3 finish one cycle at 51: 1 cycles left
run_queue is empty, sleep for 7 ticks
dispatch thread#3 at 58: allocated_time=15
thread#3 finish one cycle at 73: 0 cycles left"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')
    
@test(2, "task5")
def test_uthread():
    r.run_qemu(shell_script([
        'rttask5 EDF_CBS'
    ]), make_args = ["SCHEDPOLICY=THREAD_SCHEDULER_EDF_CBS"])
    expected = """dispatch thread#2 at 0: allocated_time=5
dispatch thread#1 at 5: allocated_time=5
dispatch thread#2 at 10: allocated_time=3
thread#2 finish one cycle at 13: 1 cycles left
dispatch thread#1 at 13: allocated_time=2
thread#1 finish one cycle at 15: 1 cycles left
run_queue is empty, sleep for 5 ticks
dispatch thread#2 at 20: allocated_time=5
dispatch thread#1 at 25: allocated_time=5
dispatch thread#2 at 30: allocated_time=3
thread#2 finish one cycle at 33: 0 cycles left
dispatch thread#1 at 33: allocated_time=2
thread#1 finish one cycle at 35: 0 cycles left"""
    if not re.findall(expected, r.qemu.output, re.M):
        raise AssertionError('Output does not match expected output')

run_tests()
os.system("make -s --no-print-directory clean")
