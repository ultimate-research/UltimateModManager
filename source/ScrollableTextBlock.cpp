#include "ScrollableTextBlock.hpp"

namespace pu::ui::elm
{
    ScrollableTextBlock::ScrollableTextBlock(s32 X, s32 Y, String Text, s32 FontSize) : TextBlock::TextBlock(X,Y,Text,FontSize) {}

    ScrollableTextBlock::~ScrollableTextBlock() {}
    
    void ScrollableTextBlock::OnInput(u64 Down, u64 Up, u64 Held, bool Touch) {
        if (Down & KEY_DDOWN || Down & KEY_LSTICK_DOWN) {
            if (this->GetTextHeight() > 720) {
                s32 newY = this->GetY() - this->GetTextHeight() * 0.1;
                if (newY < -1 * this->GetTextHeight() + 720) newY = -1 * this->GetTextHeight() + 720;
                this->SetY(newY);
            }
        }
        else if (Down & KEY_DUP || Down & KEY_LSTICK_UP) {
            s32 newY = this->GetY() + this->GetTextHeight() * 0.1;
            if (newY > 0) newY = 0;
            this->SetY(newY);
        }
    }
}