#include <cstddef>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <set>
#include <queue>

#include "drawing-manager.h"
#include "gl.h"
#include "rect.h"
#include "simple-window.h"

using namespace math;

template <typename underlying_linear_container>
class proxy2d {
public:
    proxy2d(underlying_linear_container &container,
            int col_len, int col_index):
        container_(container),
        row_index_(col_index), col_len_(col_len) {}

    decltype(auto) operator[](int col_index) {
        return container_[row_index_ * col_len_ + col_index];
    }

private:
    underlying_linear_container &container_;
    int row_index_, col_len_;
};


template <typename underlying_linear_container>
class wrap2d {
public:
    wrap2d(underlying_linear_container &&container, int col_len):
        col_len_(col_len), container_(std::move(container)) {}

    proxy2d<std::vector<bool>> operator[](int row_index) {
        return { container_, col_len_, row_index };
    }

    int col_length() { return col_len_; }
    int row_length() { return container_.size() / col_len_; }

    void print() {
        for (int i = (int) row_length() - 1; i >= 0; -- i) {
            for (int j = 0; j < (int) col_length(); ++ j)
                std::cout << (*this)[i][j];

            std::cout << "\n";
        }
    }

private:
    int col_len_;
    underlying_linear_container container_;
};


class bitset2d: private wrap2d<std::vector<bool>> {
public:
    using wrap2d<std::vector<bool>>::operator[];

    using wrap2d<std::vector<bool>>::col_length;
    using wrap2d<std::vector<bool>>::row_length;

    using wrap2d<std::vector<bool>>::print;

    bitset2d(int row_len, int col_len):
        wrap2d(std::vector<bool>(row_len * col_len), col_len) {}
};

enum directions { up, down, right, left };

class tile {
public:
    tile(int x, int y, int width, int height, math::vec3 color):
        x_(x), y_(y), width_(width), height_(height), color_(color) {}

    std::vector<tile> possible_movements(bitset2d &freeze) const {
        bool possibilities[] = { 1, 1, 1, 1 };

        std::vector<tile> tiles = {}; // TODO: inefficient

        for (int j = x_; j < x_ + width_; ++ j)
            possibilities[up]    &= y_ + height_ < freeze.row_length() && !freeze[y_ + height_][j];

        if (possibilities[up])
            tiles.emplace_back(x_, y_ + 1, width_, height_, color_);

        for (int j = x_; j < x_ + width_; ++ j)
            possibilities[down]  &= y_           >= 1                  && !freeze[y_ - 1][j];

        if (possibilities[down])
            tiles.emplace_back(x_, y_ - 1, width_, height_, color_);

        for (int i = y_; i < y_ + height_; ++ i) {
            // std::cout << freeze.col_length() << "\n";
            possibilities[right] &= x_ + width_  < freeze.col_length() && !freeze[i][x_ + width_];
        }

        if (possibilities[right])
            tiles.emplace_back(x_ + 1, y_, width_, height_, color_);

        for (int i = y_; i < y_ + height_; ++ i)
            possibilities[left]  &= x_           >= 1                  && !freeze[i][x_ - 1];

        if (possibilities[left])
            tiles.emplace_back(x_ - 1, y_, width_, height_, color_);

        return tiles;
    }

    void fill(bitset2d &freeze, int bit) const {
        for (int i = x_; i < x_ + width_; ++ i)
            for (int j = y_; j < y_ + height_; ++ j)
                freeze[j][i] = bit;
    }

    auto operator<=>(const tile& other) const {
        return std::tie(x_, y_, width_, height_) <=> std::tie(other.x_, other.y_, other.width_, other.height_);
    }

    int x_, y_;
    int width_, height_;

    math::vec3 color_ = math::random;
};

class board {
public:
    board(std::set<tile> tiles, int width, int height):
        freeze_(width, height), tiles_(tiles) {

        for (auto &&tile: tiles_)
            tile.fill(freeze_, 1);
    }

    std::vector<board> get_possible_successors() {
        std::vector<board> possible_successors;

        for (auto &&tile: tiles_) {
            auto tiles = tile.possible_movements(freeze_);
            for (auto &&movement: tiles) {
                std::set<::tile> new_tiles = tiles_;

                // preserve tile color:
                movement.color_ = tile.color_;
                new_tiles.erase(tile);

                new_tiles.insert(movement);
                possible_successors.emplace_back(std::move(new_tiles),
                    freeze_.row_length(), freeze_.col_length());
            }
        }

        return possible_successors;
    }

    std::set<tile> &tiles() { return tiles_; }

    double get_dist() const {
        const int init_x = 2;
        const int init_y = 2;

        double dist1 = 0;
        for (auto &&tile: tiles_) 
            if (tile.height_ == 2 && tile.width_ == 2) {
                dist1 = std::pow(tile.x_ - init_x, 2.0f) + std::pow(tile.y_ - init_y, 2.0f);
                break;
            }

        return dist1;
    }

    auto operator<=>(const board &other) const {
        const int init_x = 2, init_y = 2;

        double dist1 = get_dist();
        double dist2 = other.get_dist();

        return std::tie(dist1, tiles_) <=> std::tie(dist2, other.tiles_);
    }

    void print_serialized() {
        for (auto &tile: tiles_) {
            vec converted_color = tile.color_ * vec { 255, 255, 255 };
            vec<int, 3> casted_color = vec {
                (int) converted_color.r(),
                (int) converted_color.g(),
                (int) converted_color.b()
            };

            std::cout << tile.x_ << " " << tile.y_ << " "
                      << tile.width_      << " "
                      << tile.height_     << " "
                      << casted_color.r() << " "
                      << casted_color.g() << " "
                      << casted_color.b() << "; ";
        }

    }

private:
    bitset2d freeze_;
    std::set<tile> tiles_;
};

board generate_derivative_boards(board source) {
    std::set<board> visited;

    std::priority_queue<board> staged;
    staged.push(source);

    int count = 0;
    while (!staged.empty()) {
        auto current_board = staged.top();
        visited.insert(current_board);

        staged.pop();

        for (auto &&board: current_board.get_possible_successors())
            if (!visited.contains(board))
                staged.push(board);

        if (count > 1000000) // We don't need that many
            break;

        std::cout << "Generated: " << ++ count << "\r";
        ++ count;
    }

    return staged.top();
}

class my_window: public gl::simple_drawing_window {
public:
    my_window(std::set<tile> tiles, int width, int height):
        simple_drawing_window(1080 * height / (double) width, 1080, "Visualizer | OpenGL"),
        current_(std::move(tiles), width, height),
        width_(width), height_(height) {

        auto successors = generate_derivative_boards(current_);
        possible_ = std::vector { successors };
        // possible_ = std::vector { current_ };
    }

    void loop_draw(gl::drawing_manager mgr) override {
        const vec step = { 2.0f / height_, 2.0f / width_ };

        const math::vec spacing = { 0.003f, 0.003f };

        mgr.set_outer_color({ 0.1f, 0.1f, 0.1f });
        for (int i = 0; i < height_; ++ i) 
            for (int j = 0; j < width_; ++ j) {
                axes current_cell = {
                    vec { 1.0f, 1.0f } - (step * vec { i,     j     } + spacing),
                    vec { 1.0f, 1.0f } - (step * vec { i + 1, j + 1 } - spacing)
                };

                draw_cell(mgr.with_applied(current_cell));
            }

        for (auto &tile: possible_[counter_ % possible_.size()].tiles()) {
            mgr.set_outer_color(tile.color_);
            for (int i = tile.x_; i < tile.x_ + tile.width_; ++ i)
                for (int j = tile.y_; j < tile.y_ + tile.height_; ++ j) {
                    axes current_cell = {
                        vec { 1.0f, 1.0f } - (step * vec { i,     j     } + spacing),
                        vec { 1.0f, 1.0f } - (step * vec { i + 1, j + 1 } - spacing)
                    };

                    draw_cell(mgr.with_applied(current_cell));
                }
        }
    }

    void on_fps_updated() override { counter_ ++; }

private:
    board current_;
    std::vector<board> possible_;

    int counter_ = 0;
    int width_, height_;

    static void draw_cell(gl::drawing_manager mgr) {
        mgr.draw_rectangle({ -1.0f, -1.0f }, { 1.0f, 1.0f });
    }
};

int main() {
    std::set<tile> tiles = {
        tile(0, 0, 2, 1, { 109/255.f, 126/255.f, 182/255.f }),
        tile(2, 0, 1, 2, { 109/255.f, 126/255.f, 182/255.f }),
        tile(0, 4, 2, 1, { 109/255.f, 126/255.f, 182/255.f }),
        tile(2, 4, 2, 1, { 109/255.f, 126/255.f, 182/255.f }),

        tile(2, 2, 2, 2, { 205/255.f, 181/255.f, 109/255.f }),

        tile(0, 2, 1, 1, {   6/255.f,  42/255.f, 139/255.f }),
        tile(0, 3, 1, 1, {   6/255.f,  42/255.f, 139/255.f }),
        tile(1, 2, 1, 1, {   6/255.f,  42/255.f, 139/255.f }),
        tile(1, 3, 1, 1, {   6/255.f,  42/255.f, 139/255.f }),
    };

    my_window win(std::move(tiles), 5, 4);
    win.draw_loop();

    // board won_position(std::move(tiles), 5, 5);
    // auto derivatives_set = generate_derivative_boards(won_position);

    // std::vector derivatives(derivatives_set.begin(), derivatives_set.end());

    // std::srand(time(0));
    // auto chosen_one = derivatives[rand() % derivatives.size()];

    // chosen_one.print_serialized();
}
