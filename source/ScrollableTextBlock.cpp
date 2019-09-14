#include "ScrollableTextBlock.hpp"

namespace pu::ui::elm
{
    ScrollableTextBlock::ScrollableTextBlock(s32 X, s32 Y, String Text, s32 FontSize, bool AutoScroll) : TextBlock::TextBlock(X,Y,Text,FontSize) 
    {
        this->autoScroll = AutoScroll;
        this->prevY = Y;
        this->SetText("single line");
        this->singleLineHeight = this->GetHeight();
        this->SetText(Text);
    }

    ScrollableTextBlock::~ScrollableTextBlock() {}

    void ScrollableTextBlock::SetText(String Text)
    {
        s32 prevHeight = this->GetHeight();
        TextBlock::SetText(Text);
        s32 height = this->GetHeight();
        s32 oldY = this->GetY();
        s32 newY = oldY;
        if (this->autoScroll && height > 720) {
           newY = oldY - (height - prevHeight);
           this->SetY(newY);
        }

        this->prevY = oldY;
    }
    
    void ScrollableTextBlock::OnInput(u64 Down, u64 Up, u64 Held, bool Touch) {
        s32 height = this->GetHeight();
        s32 oldY = this->GetY();
        s32 newY = oldY;
        if (Held & KEY_DDOWN || Held & KEY_LSTICK_DOWN) {
            if (height > 720) {
                newY = oldY - height * 0.1;
                if (newY < -1 * height + 720) newY = -1 * height + 720;
                this->SetY(newY);
            }
        }
        else if (Held & KEY_DUP || Held & KEY_LSTICK_UP) {
            newY = oldY + height * 0.1;
            if (newY > 0) newY = 0;
            this->SetY(newY);
        }

        this->prevY = oldY;
    }
}