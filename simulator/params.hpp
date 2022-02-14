#pragma once

inline constexpr int op_type_num = 39;

inline constexpr unsigned long long max_op_count = 10000000000;
inline constexpr int stack_border = 1000;

inline constexpr unsigned int pipelined_fpu_stage_num = 3;

inline constexpr unsigned int index_width = 14;
inline constexpr unsigned int offset_width = 2;

inline constexpr unsigned int gshare_width = 12;

inline constexpr unsigned int frequency = 122500000;
inline constexpr unsigned int baud_rate = 12000000;
inline constexpr unsigned int transmission_speed = baud_rate / 10 * 8;
inline constexpr unsigned int minrt_filesize = 47468;
inline constexpr double transmission_time = static_cast<double>(minrt_filesize) / static_cast<double>(baud_rate);
inline constexpr unsigned int cycles_when_missed = 70;
