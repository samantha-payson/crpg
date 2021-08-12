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

#include "gfx.h"

int main() {
  gfx::Engine engine {
    "VK_LAYER_KHRONOS_validation"
  };

  engine.init();

  bool shutdown = false;
  SDL_Event event;

  while (!shutdown) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
	shutdown = true;
	break;

      case SDL_KEYDOWN:
	if (event.key.keysym.sym == SDLK_q) {
	  shutdown = true;
	} break;
      default:
	break;
      }
    }
    engine.draw();
  }

  std::cout << "frames drawn: " << engine.framesDrawn() << std::endl;

  engine.cleanup();
  return 0;
}
