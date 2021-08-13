/*-
 * Copyright (c) 2021 Samantha Payson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "str-id.h"

// Read the entire contents of the file at `path` into a vector and
// return that vector.
static std::vector<char>
readFile(const std::string &path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "failed to open file '" << path << "'" << std::endl;
    std::exit(-1);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());

  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

void preproc(IDDB & db, std::ostream & out, std::vector<char> in) {
  const size_t smallest = 6; // strlen("ID("")")
  
  for (size_t i = 0; i < in.size(); i++) {
    size_t remaining = in.size() - i;

    if (remaining >= smallest && in[i+0] == 'I' && in[i+1] == 'D' && in[i+2] == '(' && in[i+3] == '\"') {
      std::string str;

      size_t j;

      for (j = i + 4; in[j] != '\"'; j++) {
	if (j+2 == in.size()) {
	  std::cerr << std::endl
		    << "    error: EOF while scanning ID(\"..." << std::endl
		    << std::endl;
	  std::exit(-1);
	} else if (j+2 == in.size()) {
	  std::cerr << std::endl
		    << "    error: newline while scanning ID(\"..." << std::endl
		    << std::endl;
	  std::exit(-1);
	} else {
	  str.push_back(in[j]);
	}
      }

      if (in[j + 1] != ')') {
	std::cerr << std::endl
		  << "    error: malformed ID(\"...\") expression, got ID(\"" << str << "\"" << in[j + 1] << "..." << std::endl
		  << std::endl;
	std::exit(-1);
      } else {
	i = j + 1;
	out << "((uint32_t)" << db.getID(str) << ")";
      }
    } else {
      out << in[i];
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << std::endl
	      << "    usage: str-id <db-name> <cmd> <args...>" << std::endl
	      << std::endl;
      std::exit(-1);
  }

  std::string cmd(argv[2]);

  IDDB db = IDDB::fromFile(argv[1]);

  if (cmd == "lookup") {
    if (argc != 4) {
      std::cerr << std::endl
		<< "    usage: str-id <db-name> lookup <str>" << std::endl
		<< std::endl;
      std::exit(-1);
    }

    std::cout << db.getID(argv[3]) << std::endl;
  } else if (cmd == "preproc") {
    auto in = readFile(argv[3]);

    preproc(db, std::cout, std::move(in));
  } else {
    std::cerr << std::endl
	      << "    '" << cmd << "' is not a valid command" << std::endl
	      << std::endl;
  }

  db.write(argv[1]);

  return 0;
}
