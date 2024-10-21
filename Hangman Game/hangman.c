#include <iom128v.h>
#include <avrdef.h>
#include <macros.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define BEGIN 0
#define LVUP 1
#define WIN 2
#define LOSE 3
#define GAME 4

/*USART 관련 함수*/
void Init_USART0(void); //usart 초기화
void putch_USART0(char data);
char getch_USART0(void);

/*LCD 관련 함수*/
void delay_ms(unsigned int m);
void delay_us(unsigned int u);
void write_data(char d);
void write_instruction(char i);
void init_lcd(void);
void LCD_STR(char* str);
void LCD_char(char c);
void LCD_pos(unsigned char row, unsigned char col);
void erase_lcd(void);
void LCD_MODE(int mode);

/*게임 관련 함수*/
void life_LED(void); //LED 목숨 표시 함수
void main_game(void); //메인 게임
void init(void); // 레벨업/게임 패배/ 게임 승리 이벤트 발생 시 초기화
void lose(void); //게임 패배 시 실행되는 함수
void win(void); //게임 승리 시 실행되는 함수
void LV_UP(void); //레벨 업 시 실행되는 함수

/*외부 인터럽트*/
void int0_isr(void); //외부 인터럽트 0 -> 누를 경우 메인 화면에서 게임 시작
void int1_isr(void); //외부 인터럽트 1 -> 누를 경우 힌트 전송
void int2_isr(void); //외부 인터럽트 2 -> 누를 경우 힌트2 전송
void int3_isr(void); //외부 인터럽트 3 -> 누를 경우 목숨 증가시킴

/*게임 변수*/
int hint_cnt = 0; //인터럽트 2에서
int ans = 0; //사용자가 단어의 문자를 몇 개 맞추었는지 카운트하는 변수
int level = 1; //레벨 - 총 6단계
int check = 0, die_cnt = 5, start = 1, go = 0;
//check : 사용자가 문자를 맞췄는지 검사하는 변수
//die_cnt : 목숨 총 5개
//start : 처음 시작은 HANGMAN으로 한다.
//go : 외부 인터럽트 0에서 사용되는 변수

int second = 61; //초기 시간 (60초)
int ran = 0; //단어장 인덱스로 사용 될 변수
char data; //사용자가 입력한 문자를 저장하는 변수
int cnt = 0; //외부 인터럽트 3에서 사용되는 변수

/*단어*/
char selected[7] = { 0 }; //게임에서 사용자가 입력한 단어
int idx = 0; //selected 배열의 인덱스
int flag = 0; //사용자가 입력한 단어인지 체크하는 변수
char answer[] = { "_____" }; //정답 _ _ _ _ _ 표시
char HANGMAN[] = { "HANGMAN" }; //처음 시작 단어
char word[5][6] = { "study", "ocean", "earth", "house", "after" };

/*힌트 - 외부 인터럽트 1 발생 시 CLCD로 문자 힌트 전송*/
char hint[5][6] = { "s___y", "o___n", "e___h", "h___e", "a___r" };

/*힌트 - 외부 인터럽트 2 발생 시 CLCD로 문자의 뜻 힌트 전송*/
char hint2[5][30] = {
    "prepare for test",
    "JEJU ISLAND",
    "Planet we live",
    "go after school",
    "reverse of before"
};


//외부 인터럽트 3 누를 경우 다시 게임 화면으로 돌아감
void main(void) {
    /*남은 목숨 표현 - G포트 0~4 LED 5개)*/
    DDRG = 0x1f;
    /*승리 시 소리 내기*/
    DDRA = 0x01;
    /*CLCD 설정*/
    DDRB = 0x07; //B포트 0번핀~2번핀을 출력으로 설정
    DDRC = 0xff; //C포트 모든 핀을 출력으로 설정
    /*CLCD 초기화*/
    init_lcd();

    /*인터럽트 설정*/
    DDRD = 0x00; //D포트 인터럽트 사용
    EICRA = 0xAA; //INT0~3사용(falling edge triggered)
    EIMSK = 0x0f; //외부 인터럽트 0~3 허용

    PORTG = 0x1f; //처음에 LED 불 켜짐
    /*USART 초기화*/
    Init_USART0(); //USART 초기화
    SEI(); // 글로벌 인터럽트 허용
    LCD_MODE(BEGIN); //초기 화면 표시(행맨 게임)
    while (1) {
        if (go == 1) { //게임 시작
            LCD_MODE(GAME); //게임 화면 표시
            while (1) {
                main_game(); //메인 게임 함수 호출하여 진행
                if (go == 0) // go변수가 0 (승리 혹은 패배) 일 경우
                    break; //반복문 나옴
            }
        }
    }
}


void main_game(void) {
    life_LED(); //목숨 표시
    data = getch_USART0(); //data에 키보드로부터 문자 입력받기
    putch_USART0(data); //입력한 문자를 putty화면에 표시
    if (start == 1) { //맨 처음 문제는 항상 HANGMAN 으로 설정
        for (int i = 0; HANGMAN[i] != '\0'; i++) {
            if (HANGMAN[i] == data) { //사용자가 입력한 문자가 단어 안에 있을경우
                LCD_pos(1, i);
                LCD_char(data); //그 위치에 표시
                check = 1;  //문자 맞춘것을 체크
                ans++; //맞춘 문자 개수 카운트
            }
        }
        if (check == 0) //입력한 문자가 없을 경우
            die_cnt--; //목숨 감소
        check = 0; //check 변수 초기화
        if (ans >= strlen(HANGMAN)) { //답을 맞추면
            start = 0; //그다음부터는 임의의 단어 나옴
            level++; //레벨업
            LCD_MODE(LVUP); //레벨 업 표시
            LCD_MODE(GAME); //게임 화면 다시 표시
            init(); //변수들 초기화
            PORTG = 0x1f; //LED 불 다시 켜짐
        }
    }
    else { //두번째 문제부터는 임의의 단어
        for (int i = 0; word[ran][i] != '\0'; i++) {
            if (word[ran][i] == data) {  //사용자가 입력한 문자가 단어 안에 있을 경우
                for (int j = 0; j < 5; j++) { //사용자가 이미 입력했던 문자인지 확인
                    if (selected[j] == data) { //입력했을경우
                        flag = 1; //flag = 1로 설정
                        break;
                    }
                }
                if (flag == 1) { //입력한 문자일 경우
                    check = 1; //check를 1로 만들고 건너 뛴다
                    break;
                }
                selected[idx++] = data; //입력안한 문자일 경우 selected 배열에 추가
                LCD_pos(1, i);
                LCD_char(data); //문제 위치에 표시
                check = 1;  //문자 맞춘것을 체크
                ans++; //맞춘 문자 개수 카운트
            }
        }
        if (check == 0)//입력한 문자가 없을 경우
            die_cnt--; //목숨 감소
        check = 0;
        flag = 0;
        if (level == 6 && ans == strlen(word[ran])) {
            LCD_MODE(WIN); //모든 문제를 다 맞출경우, LCD에 승리 화면 표시
            PORTA = 0x01; //액티브 부저 소리 나게 설정
            delay_ms(300);
            PORTA = 0x00; //액티브 부저 소리를 다시 끈다
            ran = 0; //단어 초기화
            level = 1; //레벨 초기화
            cnt = 0; //카운트 초기화
            init(); //각종 변수 초기화
            start = 1; //HANGMAN 단어부터 다시 나오게 설정
            go = 0; //go 변수가 0으로 바뀌어 main함수의 반복문을 나간다
            LCD_MODE(BEGIN);  //게임 시작 화면으로 돌아간다
        }
        else if (ans >= strlen(word[ran])) { //답을 맞출 경우
            for (int i = 0; i < 5; i++)
                selected[i] = 0; //배열 초기화
            idx = 0;  //selected 배열의 인덱스로 사용할 변수 초기화
            level++; //레벨업
            ran++; //다음 단어 선택
            LCD_MODE(LVUP); //레벨업 화면 표시
            LCD_MODE(GAME); //다시 게임화면으로 돌아감
            init(); //각종 변수 초기화
        }
    }
    if (die_cnt == 0) {
        PORTG = 0x00;
        LCD_MODE(LOSE); //패배 화면 표시
        delay_ms(300);
        ran = 0; //단어 초기화
        cnt = 0; //카운트 초기화
        level = 1; //레벨 초기화
        init(); //각종 변수 초기화
        start = 1; //HANGMAN 단어부터 다시 나오게 설정
        go = 0; //go 변수가 0으로 바뀌어 main함수의 반복문을 나간다
        LCD_MODE(BEGIN); //게임 시작 화면으로 돌아간다
    }
}


void init(void) {
    die_cnt = 5; //목숨 초기화
    check = 0;
    ans = 0; // 맞춘 문자 수 초기화
}


void life_LED(void) { //LED로 목숨 개수를 표시하는 함수
    switch (die_cnt) {
    case 0: //게임 오버
        PORTG = 0x00;
        break;
    case 1: //목숨 1개
        PORTG = 0x01;
        break;
    case 2: //목숨 2개
        PORTG = 0x03;
        break;
    case 3: //목숨 3개
        PORTG = 0x07;
        break;
    case 4: //목숨 4개
        PORTG = 0x0f;
        break;
    case 5: //목숨 5개
        PORTG = 0x1f;
        break;
    }
    /*목숨 수 만큼 LCD에도 O로 표시*/
    for (int i = 0; i < die_cnt; i++) {
        LCD_pos(0, 11 + i);
        LCD_char('O');
    }
    for (int i = 0; i < 5 - die_cnt; i++) {
        LCD_pos(0, 15 - i);
        LCD_char(' ');
    }
}


void Init_USART0(void) {
    //RXCIE0 : 수신 완료 인터럽트 허가
    //UDRIE0 : 데이터 레지스터 준비완료 인터럽트 허가
    //RXEN0 : 수신 허가
    //TXEN0 : 송신 허가
    UCSR0B = (1 << RXCIE0) | (1 << UDRIE0) | (1 << RXEN0) | (1 << TXEN0);
    UCSR0A = 0x00;
    //문자길이 8비트
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UBRR0H = 0x00;
    UBRR0L = 0x67; //보드레이트 9600 설정
    //   SEI(); // SREG |= 0x80;
}


/*MCU -> putty 화면으로 전송하기*/
void putch_USART0(char data) {
    while (!(UCSR0A & (1 << UDRE0))); //데이터를 쓸 준비 완료가 되면
    UDR0 = data; //데이터 전송
}


/*putty -> MCU 로 입력하기*/
char getch_USART0(void) {
    while (!(UCSR0A & (1 << RXC0))); //수신이 완료되면
    return UDR0; //반환
}


/*각 상황별로 LCD에 출력하는 함수*/
void LCD_MODE(int mode) {
    switch (mode) {
    case BEGIN: //게임 시작 화면
        erase_lcd();
        LCD_pos(0, 0);
        LCD_STR("  HANGMAN GAME");
        LCD_pos(1, 0);
        LCD_STR("  PRESS BUTTON");
        break;
    case LVUP:  //레벨 업 화면
        erase_lcd();
        LCD_pos(0, 0);
        LCD_STR("Level UP!!!");
        delay_ms(200);
        break;
    case WIN:  //승리 화면
        erase_lcd();
        LCD_pos(0, 0);
        LCD_STR("YOU WIN!!!");
        PORTA = 0x01;
        break;
    case LOSE: //패배 화면
        erase_lcd();
        LCD_pos(0, 0);
        LCD_STR("GAME OVER");
        LCD_pos(1, 0);
        LCD_STR("Try Again");
        break;
    case GAME: //메인 게임 화면
        erase_lcd();
        LCD_pos(0, 0);
        LCD_STR("Lv.");
        LCD_char(level + '0');
        LCD_pos(1, 0);
        if (start == 1)
            LCD_STR("_______");
        else
            LCD_STR(answer);
        break;
    }
}


/*어느 위치에 디스플레이 할 지 정하는 함수*/
/*instruction 함수에서 0번핀, 1번핀은 'L' 상태로 한다*/
void write_instruction(char i) {
    PORTB = 0x04; //instruction 모드는 RS = 0으로 설정한다.
    delay_us(10);
    PORTC = i; //위치 값 전달
    delay_us(10);
    PORTB = 0x00; //2번 핀인 Enable 1->0(하강 에지)가 됨
    delay_us(100);
}


/*어떤 문자를 출력할 지 정하는 함수*/
/*write 함수에서 0번핀은 'H', 1번핀은 'L' 상태로 한다*/
void write_data(char d) {
    PORTB = 0x05; //write 모드는 RS = 1 로 설정한다.
    delay_us(100);
    PORTC = d; //출력할 문자 전달
    PORTB = 0x01; //2번 핀인 Enable 이 1->0(하강 에지)가 됨
    delay_us(100);
}


/*clcd 초기화 함수*/
void init_lcd(void) {
    delay_ms(10);
    write_instruction(0x30); //기능 설정
    delay_ms(25);
    write_instruction(0x30); //기능 설정
    delay_ms(5);
    write_instruction(0x30); //기능 설정
    delay_ms(5);
    write_instruction(0x3c); //기능 설정
    delay_ms(5);
    write_instruction(0x08); //표시 온,오프
    delay_ms(5);
    write_instruction(0x01); //표시 클리어
    delay_ms(5);
    write_instruction(0x06); //엔트리모드 설정
    delay_ms(5);
    write_instruction(0x0c); //표시 온,오프 설정
    delay_ms(15);
}


/*딜레이 함수*/
void delay_us(unsigned int u) {
    unsigned int i, j;
    for (i = 0; i < u; i++) {
        for (j = 0; j < 2; j++);
    }
}


/*딜레이 함수*/
void delay_ms(unsigned int m) {
    unsigned int i, j;
    for (i = 0; i < m; i++) {
        for (j = 0; j < 2100; j++)
            ;
    }
}


/*문자열을 출력하는 함수*/
void LCD_STR(char* str) {
    for (int i = 0; str[i] != 0; i++) {
        write_data(str[i]);
    }
}


/*문자 하나를 출력하는 함수*/
void LCD_char(char c) {
    write_data(c);
    delay_ms(5);
}


/*행,열을 입력하면 그 위치에 CLCD의 주소값을 지정해주는 함수*/
void LCD_pos(unsigned char row, unsigned char col) {
    write_instruction(0x80 | (row * 0x40 + col)); //행(row)과 열(col)
}


/*LCD를 모두 지우는 함수*/
void erase_lcd(void) {
    LCD_pos(0, 0);
    LCD_STR("                   ");
    LCD_pos(1, 0);
    LCD_STR("                   ");
}


//인터럽트 0 - 게임 시작
#pragma interrupt_handler int0_isr:iv_INT0
void int0_isr(void) {
    go = 1; //누르면 게임 시작
}


//인터럽트 1 - 문자 힌트 전송
#pragma interrupt_handler int1_isr:iv_INT1
void int1_isr(void) {
    if (start == 0) { //두번째 문제부터 힌트 사용 가능
        ans += 2; //힌트 사용 시 맞춘 문자 2개 증가
        /*LCD에 표시*/
        LCD_pos(1, 0);
        LCD_char(hint[ran][0]);
        LCD_pos(1, 4);
        LCD_char(hint[ran][4]);
        for (int i = 0; i < 5; i++) { //만약에 힌트가 사용자가 이미 맞춘 문자일 경우 맞춘 문자수에서 제외함
            if (selected[i] == hint[ran][0] || selected[i] == hint[ran][4])
                ans--;
        }
        selected[idx++] = hint[ran][0];
        selected[idx++] = hint[ran][4];
    }
}


//인터럽트 2 - 문장 힌트 전송
#pragma interrupt_handler int2_isr:iv_INT2
void int2_isr(void) {
    if (start == 0) { //두번째 단어부터 힌트 전송 가능
        hint_cnt++; //인터럽트 발생 시 cnt 증가
        if (hint_cnt == 1) { //cnt가 1이면 힌트 전송 후 LCD에 표시
            erase_lcd();
            LCD_pos(0, 0);
            LCD_STR(hint2[ran]);
        }
        if (hint_cnt == 2) { //한번 더 누를 경우
            hint_cnt = 0; //cnt가 초기화되고
            erase_lcd();  //원래 게임 화면으로 돌아옴
            LCD_MODE(GAME);
            life_LED(); //목숨 표시
            /*게임 화면 다시 표시*/
            for (int i = 0; i < 5; i++) {
                if (selected[i] != 0) {
                    for (int j = 0; j < 5; j++) {
                        if (word[ran][j] == selected[i]) {
                            LCD_pos(1, j);
                            LCD_char(selected[i]);
                        }
                    }
                }
            }
        }
    }
}


//인터럽트 3 - 목숨 개수 증가 (3번까지만 가능)
#pragma interrupt_handler int3_isr:iv_INT3
void int3_isr(void) {
    if(cnt < 3){
        if(die_cnt < 5)
            die_cnt++;
        life_LED(); //목숨 개수를 표시하는 함수
    }
    cnt++;
}


#pragma interrupt_handler uart0_tx_isr:iv_USART0_TXC
void uart0_tx_isr(void) {
}


#pragma interrupt_handler uart0_rx_isr:iv_USART0_RXC
void uart0_rx_isr(void) {
}