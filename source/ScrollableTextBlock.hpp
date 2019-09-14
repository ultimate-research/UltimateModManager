#include <pu/Plutonium>

namespace pu::ui::elm
{
    class ScrollableTextBlock : public pu::ui::elm::TextBlock
    {
        public:
            ScrollableTextBlock::ScrollableTextBlock(s32 x, s32 y, pu::String Text, s32 FontSize = 25, bool AutoScroll = true);
            ~ScrollableTextBlock();
            void ScrollableTextBlock::SetText(String Text) override;
            // Plutonium's macro to define a constructor static function for smart pointers
            // Would be similar to: static std::shared_ptr<Layout1> Layout1::New()
            PU_SMART_CTOR(ScrollableTextBlock)

            void OnInput(u64 Down, u64 Up, u64 Held, bool Touch);
            bool autoScroll;
            s32 prevY;
            s32 singleLineHeight;
    };
}