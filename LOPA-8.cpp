#include <stdint.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <cstdint>

class Memory
{
    private:
        uint8_t rom[16384] = {};
        uint8_t ram[47872] = {};
        uint8_t kb[256] = {};
        uint8_t vram[1024] = {};
    public:
        Memory();
        uint8_t read(uint16_t address);
        void write(uint16_t address, uint8_t value);
        void load_ROM(const char* filename);
};

class CPU
{
private:
    uint8_t A;
    uint8_t B;
    uint8_t C;
    uint8_t AC;
    uint16_t PC;
    uint16_t PTR;
    uint8_t FLAGS;
    uint16_t SP;
    uint16_t RA;

    Memory& memory;
public:
    CPU(Memory& memory);
    void reset();
    void cycle();
};

class Monitor
{
private:
    HWND window;

    static constexpr int WIDTH = 128;
    static constexpr int HEIGHT = 64;
    static constexpr int SCALE = 1;

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        Monitor* monitor = reinterpret_cast<Monitor*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA)
        );

        switch (message)
        {
            case WM_NCCREATE:
            {
                CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(lParam);

                Monitor* monitor = reinterpret_cast<Monitor*>(create->lpCreateParams);

                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(monitor));

                return TRUE;
            }

            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_PAINT:
            {
                PAINTSTRUCT ps;

                HDC hdc = BeginPaint(hwnd, &ps);

                for (int y = 0; y < 64; y++)
                {
                    for (int byte = 0; byte < 16; byte++)
                    {
                        uint8_t data = monitor->memory.read(0xFC00 + (y * 16) + byte);

                        for (int bit = 0; bit < 8; bit++)
                        {
                            int x = (byte * 8) + bit;

                            if (data & (1 << bit))
                            {
                                SetPixel(hdc, x, y, RGB(255, 255, 255));
                            }
                            else
                            {
                                SetPixel(hdc, x, y, RGB(0, 0, 0));
                            }
                        }
                    }
                }

                EndPaint(hwnd, &ps);

                return 0;
            }
        }

        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    Memory& memory;

public:
    Monitor(Memory& memory) : memory(memory)
    {
        HINSTANCE instance = GetModuleHandle(nullptr);

        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = instance;
        wc.lpszClassName = "LOPA8Monitor"; //compile specifc dont fix if only ide complains

        RegisterClass(&wc);

        window = CreateWindowEx(
            0,
            "LOPA8Monitor", //compile specifc dont fix if only ide complains
            "LOPA-8", //compile specifc dont fix if only ide complains
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            WIDTH * SCALE,
            HEIGHT * SCALE,
            nullptr,
            nullptr,
            instance,
            this
        );

        ShowWindow(window, SW_SHOW);
    }

    void update()
    {
        MSG message;

        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        InvalidateRect(window, nullptr, TRUE);
    }
};

class Keyboard
{
private:
    static constexpr uint16_t KB_BASE = 0xFB00;

    Memory& memory;

public:
    Keyboard(Memory& memory) : memory(memory)
    {
    }

    void update()
    {
        // A-Z
        for (int i = 0; i < 26; i++)
        {
            memory.write(
                KB_BASE + i,
                (GetAsyncKeyState('A' + i) & 0x8000) ? 1 : 0
            );
        }

        // 1-9
        for (int i = 0; i < 9; i++)
        {
            memory.write(
                KB_BASE + 26 + i,
                (GetAsyncKeyState('1' + i) & 0x8000) ? 1 : 0
            );
        }

        // 0
        memory.write(
            KB_BASE + 35,
            (GetAsyncKeyState('0') & 0x8000) ? 1 : 0
        );

        // Shift
        memory.write(
            KB_BASE + 36,
            ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) ||
             (GetAsyncKeyState(VK_RSHIFT) & 0x8000))
                ? 1
                : 0
        );
    }
};

enum Opcode
{
    MOV = 0,
    MFM = 1,
    MTM = 2,
    PUSH = 3,
    POP = 4,
    ADD = 5,
    SUB = 6,
    DIV = 7,
    MUL = 8,
    SHL = 9,
    SHR = 10,
    AND = 11,
    OR = 12,
    XOR = 13,
    NOT = 14,
    JMP = 15,
    CMP = 16,
    JZ = 17,
    JC = 18,
    NOP = 19
};

enum Reg
{
    a = 1,
    b = 2,
    c = 3,
    ac = 4,
    ptr = 6
};

CPU::CPU(Memory& memory) : memory(memory)
{
}

void CPU::reset()
{
    A = 0;
    B = 0;
    C = 0;
    AC = 0;
    PC = 0;
    PTR = 0;
    FLAGS = 0;
    SP = 0;
    RA = 0;
}

Memory::Memory()
{
}

uint8_t Memory::read(uint16_t address)
{
    if (address <= 0x3FFF)
    {
        return rom[address];
    }
    else if (address <= 0xFAFF)
    {
        return ram[address - 0x4000];
    }
    else if (address <= 0xFBFF)
    {
        return kb[address - 0xFB00];
    }
    else if (address <= 0xFFFF)
    {
        return vram[address - 0xFC00];
    }
}

void Memory::write(uint16_t address, uint8_t value)
{
    if (address >= 0x4000 && address <= 0xFAFF)
    {
        ram[address - 0x4000] = value;
    }
    else if (address >= 0xFC00 && address <= 0xFFFF)
    {
        vram[address - 0xFC00] = value;
    }
}

void Memory::load_ROM(const char* filename)
{
    const size_t ROM_size = 16384;

    std::ifstream file(filename, std::ios::binary);

    if (!file)
    {
        std::cerr << "Failed to open ROM file.\n";
        return;
    }

    file.read(reinterpret_cast<char*>(rom), ROM_size);
}

void CPU::cycle()
{
    uint8_t opcode = memory.read(PC);

    uint8_t OP1 = memory.read(PC + 1) >> 4;
    uint8_t OP2 = memory.read(PC + 1) & 0x0F;

    auto get8 = [&](uint8_t reg) -> uint8_t
    {
        switch (reg)
        {
            case a:  return A;
            case b:  return B;
            case c:  return C;
            case ac: return AC;
            default: return 0;
        }
    };

    auto set8 = [&](uint8_t reg, uint8_t value)
    {
        switch (reg)
        {
            case a:  A = value; break;
            case b:  B = value; break;
            case c:  C = value; break;
            case ac: AC = value; break;
        }
    };

    switch (opcode)
    {
        case MOV:
        {
            if (OP1 == ptr)
            {
                PTR = get8(OP2);
            }
            else if (OP2 == ptr)
            {
                set8(OP1, PTR);
            }
            else
            {
                set8(OP1, get8(OP2));
            }

            PC += 2;
            break;
        }

        case MFM:
        {
            set8(OP1, memory.read(PTR));

            PC += 2;
            break;
        }

        case MTM:
        {
            memory.write(PTR, get8(OP1));

            PC += 2;
            break;
        }

        case PUSH:
        {
            SP--;
            memory.write(SP, get8(OP1));

            PC += 2;
            break;
        }

        case POP:
        {
            set8(OP1, memory.read(SP));
            SP++;

            PC += 2;
            break;
        }

        case ADD:
        {
            uint16_t result = (uint16_t)get8(OP1) + (uint16_t)get8(OP2);

            FLAGS &= ~(1 << 0);

            if (result > 0xFF)
            {
                FLAGS |= (1 << 0);
            }

            AC = (uint8_t)result;

            PC += 2;
            break;
        }

        case SUB:
        {
            uint16_t result = (uint16_t)get8(OP1) - (uint16_t)get8(OP2);

            FLAGS &= ~(1 << 0);

            if (result > 0xFF)
            {
                FLAGS |= (1 << 0);
            }

            AC = (uint8_t)result;

            PC += 2;
            break;
        }

        case DIV:
        {
            uint8_t x = get8(OP1);
            uint8_t y = get8(OP2);

            FLAGS &= ~(1 << 0);

            if (x != 0)
            {
                AC = y / x;
            }
            else
            {
                AC = 0;
            }

            PC += 2;
            break;
        }

        case MUL:
        {
            uint32_t result =
                (uint32_t)get8(OP1) * (uint32_t)get8(OP2);

            FLAGS &= ~(1 << 0);

            if (result > 0xFFFF)
            {
                FLAGS |= (1 << 0);
            }

            A = (uint8_t)(result & 0xFF);
            AC = (uint8_t)((result >> 8) & 0xFF);

            PC += 2;
            break;
        }

        case SHL:
        {
            uint32_t result =
                (uint32_t)get8(OP2) << get8(OP1);

            FLAGS &= ~(1 << 0);

            if (result > 0xFFFF)
            {
                FLAGS |= (1 << 0);
            }

            A = (uint8_t)(result & 0xFF);
            AC = (uint8_t)((result >> 8) & 0xFF);

            PC += 2;
            break;
        }

        case SHR:
        {
            uint32_t result =
                (uint32_t)get8(OP2) >> get8(OP1);

            FLAGS &= ~(1 << 0);

            if (result > 0xFFFF)
            {
                FLAGS |= (1 << 0);
            }

            A = (uint8_t)(result & 0xFF);
            AC = (uint8_t)((result >> 8) & 0xFF);

            PC += 2;
            break;
        }

        case AND:
        {
            AC = get8(OP1) & get8(OP2);

            PC += 2;
            break;
        }

        case OR:
        {
            AC = get8(OP1) | get8(OP2);

            PC += 2;
            break;
        }

        case XOR:
        {
            AC = get8(OP1) ^ get8(OP2);

            PC += 2;
            break;
        }

        case NOT:
        {
            AC = ~(get8(OP1) & get8(OP2));

            PC += 2;
            break;
        }

        case JMP:
        {
            PC = PTR;
            break;
        }

        case CMP:
        {
            FLAGS &= ~(1 << 1);

            if (get8(OP1) == get8(OP2))
            {
                FLAGS |= (1 << 1);
            }

            PC += 2;
            break;
        }

        case JZ:
        {
            if (FLAGS & (1 << 1))
            {
                PC = PTR;
            }
            else
            {
                PC += 2;
            }

            break;
        }

        case JC:
        {
            if (FLAGS & (1 << 0))
            {
                PC = PTR;
            }
            else
            {
                PC += 2;
            }

            break;
        }

        case NOP:
        {
            PC += 2;
            break;
        }

        default:
        {
            PC += 2;
            break;
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

Memory memory;
CPU cpu(memory);
Monitor monitor(memory);
Keyboard keyboard(memory);

int main(int argc, char* argv[]) {
    
    if (argc < 2)
    {
        std::cout << "Usage: LOPA-8.exe <rom file>\n";
        return 1;
    }

    memory.load_ROM(argv[1]);
    
    cpu.reset();
    while (1)
    {
        monitor.update();
        keyboard.update();
        cpu.cycle();
    }

    return 0;
}
