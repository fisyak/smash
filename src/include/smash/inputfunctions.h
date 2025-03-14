/*
 *
 *    Copyright (c) 2014-2015,2017-2018,2020,2024
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_SMASH_INPUTFUNCTIONS_H_
#define SRC_INCLUDE_SMASH_INPUTFUNCTIONS_H_

#include <iostream>
#include <string>
#include <utility>

#include "forwarddeclarations.h"
#include "particletype.h"

namespace smash {

/// Line consists of a line number and the contents of that line
struct Line { /*{{{*/
  /// Initialize line with empty string and number
  Line() = default;
  /// Initialize a line with line number \p n and text \p t
  Line(int n, std::string &&t) : number(n), text(std::move(t)) {}
  /// Line number
  int number;
  /// Line content.
  std::string text;
}; /*}}}*/

/**
 * Builds a meaningful error message
 *
 * Takes the message and quotes the Line where the error occurs
 *
 * \param[in] message Error message
 * \param[in] line Line object containing line number and line content.
 */
inline std::string build_error_string(std::string message, const Line &line) {
  return message + " (on line " + std::to_string(line.number) + ": \"" +
         line.text + "\")";
}

/**
 * Helper function for parsing particles.txt and decaymodes.txt.
 *
 * This function goes through an input stream line by line and removes
 * comments and empty lines. The remaining lines will be returned as a vector
 * of strings and linenumber pairs (Line).
 *
 * \param[in] input an lvalue reference to an input stream
 */
build_vector_<Line> line_parser(const std::string &input);

/// Makes sure that nothing is left to read from this line.
inline void ensure_all_read(std::istream &input, const Line &line) { /*{{{*/
  std::string tmp;
  input >> tmp;
  if (!input.eof()) {
    throw ParticleType::LoadFailure(
        build_error_string("While loading the Particle data:\nGarbage (" + tmp +
                               ") at the remainder of the line.",
                           line));
  }
} /*}}}*/

/**
 * Utility function to read a complete input stream (e.g. file) into one string.
 *
 * \param[in] input The input stream. Since it reads until EOF und thus "uses up
 * the whole input stream" the function takes an rvalue reference to the stream
 * object (just pass a temporary).
 *
 * \note There's no slicing here: the actual istream object is a temporary that
 * is not destroyed until read_all returns.
 *
 * \warning The GNU compiler reports false positive warnings here because of
 * <a href="https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105580">this bug</a>
 * and therefore we temporary manually disable that diagnostic.
 */
#if defined(__GNUC__) && (__GNUC__ >= 12) && (__GNUC__ < 15)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
inline std::string read_all(std::istream &&input) {
  return {std::istreambuf_iterator<char>{input},
          std::istreambuf_iterator<char>{}};
}
#if defined(__GNUC__) && (__GNUC__ >= 12) && (__GNUC__ < 15)
#pragma GCC diagnostic pop
#endif

/** Check if a line in the string ends with \\r\\n. This may happen when a file
 * was edited on Windows.
 *
 * \param[in] in Input string
 * \returns True if \\r\\n was found, else false
 */
inline bool has_crlf_line_ending(const std::string in) {
  if (in.find("\r\n") != std::string::npos) {
    return true;
  }
  return false;
}

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_INPUTFUNCTIONS_H_
