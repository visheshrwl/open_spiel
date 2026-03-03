# Copyright 2026 DeepMind Technologies Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for the game-specific functions for shogi."""

from absl import flags
from absl.testing import absltest
from absl.testing import parameterized

import pyspiel
shogi = pyspiel.shogi

class GamesShogiTest(parameterized.TestCase):
    def test_bindings_sim(self):
      game = pyspiel.load_game("shogi")
      state = game.new_initial_state()
      board = state.board()
      for action in state.legal_actions():
          print(shogi.action_to_move(action, board).to_string())
      return

    def lolNope(self):

      state.apply_action(state.parse_move_to_action("g3g4"))
      state.apply_action(state.parse_move_to_action("c7c6"))
      for action in state.legal_actions():
          print(shogi.action_to_move(action, board).to_string())
      #state.apply_action(state.parse_move_to_action("h8b2"))
      print(state)



if __name__ == "__main__":
  absltest.main()


