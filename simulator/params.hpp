#pragma once

inline constexpr int op_type_num = 39;

inline constexpr unsigned long long max_op_count = 10000000000;
inline constexpr int stack_border = 1000;

inline constexpr unsigned int pipelined_fpu_stage_num = 3;

constexpr unsigned int addr_width = 25;
inline constexpr unsigned int index_width = 12;
inline constexpr unsigned int offset_width = 4;
// tag_width = 9

inline constexpr unsigned int gshare_width = 12;

inline constexpr unsigned int frequency = 121000000;
inline constexpr unsigned int baud_rate = 12000000;
inline constexpr unsigned int transmission_speed = baud_rate / 10 * 8;
inline constexpr unsigned int minrt_filesize = 48084;
inline constexpr double transmission_time = static_cast<double>(minrt_filesize) / static_cast<double>(baud_rate);
inline constexpr unsigned int cycles_when_missed = 70;
