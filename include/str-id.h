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

#ifndef CRPG_STR_ID_H
#define CRPG_STR_ID_H

#include <fstream>
#include <iostream>

#include <map>
#include <vector>

class IDDB {
public:
  static IDDB fromFile(const std::string &path) {
    std::ifstream in(path);

    if (!in.is_open()) {
      std::cerr << "Failed to open '" << path << "' as ID DB." << std::endl;
      std::exit(-1);
    }

    char buf[1024];

    IDDB db;

    while (!in.eof()) {
      in.getline(buf, 1024);
      auto str = std::string(buf);
      db.getID(str);
    }

    in.close();

    return db;
  }

  uint32_t getID(const std::string &name) {
    const auto entry = _nameToID.find(name);
    if (entry != _nameToID.end()) {
      return entry->second;
    } else {
      _idToName.push_back(name);
      _nameToID.emplace(name, _idToName.size());

      return _idToName.size();
    }
  }

  void write(const std::string &path) const {
    std::ofstream out(path);

    for (const auto &name : _idToName) {
      out << name << std::endl;
    }

    out.close();
  }

private:
  std::map<std::string, uint32_t> _nameToID;
  std::vector<std::string> _idToName;
};

#endif

