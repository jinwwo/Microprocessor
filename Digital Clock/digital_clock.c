#include <iom128v.h>
#include <avrdef.h>

/*모드 정의*/
#define CLOCK 1 //일반 디지털 시계모드
#define GMT 2  //세계 표준시 모드
#define SW 3  //스톱워치 모드
#define ALARM 4 //알람이 울릴경우 출력

/*함수 원형 선언*/
void delay_ms(unsigned int m);
void delay_us(unsigned int u);
void write_data(char d);
void write_instruction(char i);
void init_lcd(void);
void LCD_STR(char* str);
void LCD_char(char c);
void LCD_pos(unsigned char row, unsigned char col);
void ModeSet(int m);

/*인터럽트*/
void int0_isr(void);
void int1_isr(void);
void int2_isr(void);
void int3_isr(void);
void int4_isr(void);
void int5_isr(void);
void int6_isr(void);

/*타이머*/
void timer0_ovf_isr(void); //타이머0
void timer2_ovf_isr(void); //타이머2
int alarm = 1; //알람 변수
unsigned cnt = 0; //타이머 0에서 카운트하는 변수
unsigned cnt2 = 0; //타이머 2에서 카운트하는 변수

/*시계 변수*/
int hour = 16, min = 50, sec = 0; //시계 시,분,초를 저장할 변수
int c_sec1, c_min1, c_hour1; // 시계 10의자리 저장
int c_sec2, c_min2, c_hour2; // 시계 1의자리 저장

/*요일 변수*/
int day = 0;

/*세계 시계 변수*/
int z_hour; //세계 시계의 시를 저장할 변수
int zulu = 1; //세계 시간 모드 체크 변수
int z_hour1, z_hour2, zulu_day = 0; //세계 시간 시 10의자리, 1의자리, 요일을 저장할 변수

/*스톱워치 변수*/
int sw_hour = 0, sw_min = 0, sw_sec = 0, point1 = 0, point2 = 0, sw_cnt = 0;
int s_hour1 = 0, s_hour2 = 0, s_min1 = 0, s_min2 = 0, s_sec1 = 0, s_sec2 = 0;

/*요일을 담은 배열*/
char* week[] = {
  "[Sunday]        ",
  "[Monday]        ",
  "[Tuesday]       ",
  "[Wednesday]     ",
  "[Thursday]      ",
  "[Friday]        ",
  "[Saturday]      "
};

/* 세계 표준시의 요일을 담은 배열 */
char* z_week[] = {
  "[Sunday]     GMT",
  "[Monday]     GMT",
  "[Tuesday]    GMT",
  "[Wednesday]  GMT",
  "[Thursday]   GMT",
  "[Friday]     GMT",
  "[Saturday]   GMT"
};


void main(void) {
    /*CLCD 설정*/
    DDRB = 0x07; //B포트 0번핀~2번핀을 출력으로 설정
    DDRC = 0xff; //C포트 모든 핀을 출력으로 설정
    init_lcd(); //CLCD 초기화
    /*정각에 켜지는 LED 제어*/
    DDRA = 0x01; // 0번핀은 출력으로 사용, 1번핀은 입력으로 사용
    /*인터럽트 초기화(D,E포트는 입력으로 사용)*/
    DDRD = 0x00;
    DDRE = 0x00;
    EICRA = 0xAA; //INT0,1,2,3 사용(falling edge triggered)
    EICRB = 0x2A; //INT4,5,6 사용(falling edge triggered)

    EIMSK = 0x7F; //INT0,1,2,3,4,5,6 인에이블

    /*타이머 설정*/
    TCCR0 = 0x04; // 노말모드, 분주비 64
    TCCR2 = 0x0b; //ctc 모드, 분주비 64
    TIMSK = 0x01; //오버플로우 인터럽트 설정(일반 시계는 오버플로우 상시 허용)
    SEI(); //상태레지스터 I비트를 1로 설정(Global Interrupt enable)

    while (1) {
        /*
            8시에 알람 울림(제가 일어나는 시간입니다)
            알람이 꺼지면 alarm = -1이 되고, 하루가 지나면 다시 alarm = 0
            이 되어 다음날 다시 알람이 울리게 된다.
        */
        if (alarm == 0 && hour >= 8 && min >= 0) {
            PORTA = 0x01; //A포트 0번핀에 HIGH 출력
            ModeSet(ALARM); //알람 모드
        }
        else {
            c_hour1 = hour / 10; //시 10의자리
            c_hour2 = hour % 10; //시 1의자리
            c_min1 = min / 10;   //분 10의자리
            c_min2 = min % 10;   //분 1의자리
            c_sec1 = sec / 10;   //초 10의자리
            c_sec2 = sec % 10;   //초 1의자리
            z_hour = hour - 9; //세계 표준 시간은 (한국시간 - 9시간)
            if (z_hour < 0) { //세계 표준 시간이 음수가 될 경우
                z_hour += 24; //24시를 더해서 양수를 만들어줌
                zulu_day = (zulu_day - 1 + 7) % 7; //요일을 하루 전으로 해줌
            }
            /*세계 시간 시 10의 자리와 1의 자리*/
            z_hour1 = z_hour / 10;
            z_hour2 = z_hour % 10;
            /*스탑워치 시,분,초*/
            s_hour1 = sw_hour / 10; //스탑워치 시 10의자리
            s_hour2 = sw_hour % 10; //스탑워치 시 1의자리
            s_min1 = sw_min / 10; //스탑워치 분 10의자리
            s_min2 = sw_min % 10; //스탑워치 분 1의자리
            s_sec1 = sw_sec / 10; //스탑워치 초 10의자리
            s_sec2 = sw_sec % 10; //스탑워치 초 10의자리

            if (zulu % 2 != 0 && sw_cnt == 0)//한국시간 모드일 경우
                ModeSet(CLOCK);
            //세계시계 모드일 경우(한국시계와 구분하기 위해 GMT(Greenwich Mean Time) 표시기능 추가
            else if (zulu % 2 == 0 && sw_cnt == 0)
                ModeSet(GMT);
            else //스톱워치 모드일 경우
                ModeSet(SW);
            //요일 카운트
            day %= 7; //day는 0~6의 값을 가진다
            zulu_day = day; //세계 표준시계의 요일을 저장한다.
        }
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


/*mode를 설정하여 CLCD에 출력하는 함수*/
void ModeSet(int mode) {
    switch (mode) {
    case 1: //일반 시계 모드
        LCD_pos(0, 0); //0행 0열부터 출력
        LCD_STR(week[day]); //요일 출력
        if (hour < 12) { //오전
            LCD_pos(1, 0);//시 분 초 순서로 출력
            LCD_STR("AM"); //am 출력
            LCD_char(' ');
            LCD_char(c_hour1 + '0');
            LCD_char(c_hour2 + '0');
            LCD_char(':');
            LCD_char(c_min1 + '0');
            LCD_char(c_min2 + '0');
            LCD_char(':');
            LCD_char(c_sec1 + '0');
            LCD_char(c_sec2 + '0');
            LCD_STR("       ");
        }
        else if (hour >= 12) { //오후
            LCD_pos(1, 0);//시 분 초 순서로 출력
            LCD_STR("PM"); //pm 출력
            LCD_char(' ');
            LCD_char(c_hour1 + '0');
            LCD_char(c_hour2 + '0');
            LCD_char(':');
            LCD_char(c_min1 + '0');
            LCD_char(c_min2 + '0');
            LCD_char(':');
            LCD_char(c_sec1 + '0');
            LCD_char(c_sec2 + '0');
            LCD_STR("       ");
        }
        break;
    case 2: //세계 표준시 모드
        LCD_pos(0, 0);
        LCD_STR(z_week[zulu_day]); //요일 출력
        if (z_hour < 12) {
            LCD_pos(1, 0);//시 분 초 순서로 출력
            LCD_STR("AM"); //am 출력
            LCD_char(' ');
            LCD_char(z_hour1 + '0');
            LCD_char(z_hour2 + '0');
            LCD_char(':');
            LCD_char(c_min1 + '0');
            LCD_char(c_min2 + '0');
            LCD_char(':');
            LCD_char(c_sec1 + '0');
            LCD_char(c_sec2 + '0');
            LCD_STR("       ");
            LCD_pos(0, 14); //우측 상단
            LCD_STR("GMT"); //GMT 표시 기능
        }
        else if (z_hour >= 12) {
            LCD_pos(1, 0);//시 분 초 순서로 출력
            LCD_STR("PM"); //pm 출력
            LCD_char(' ');
            LCD_char(z_hour1 + '0');
            LCD_char(z_hour2 + '0');
            LCD_char(':');
            LCD_char(c_min1 + '0');
            LCD_char(c_min2 + '0');
            LCD_char(':');
            LCD_char(c_sec1 + '0');
            LCD_char(c_sec2 + '0');
            LCD_STR("       ");
            LCD_pos(0, 14); //우측 상단에
            LCD_STR("GMT"); //GMT 표시 기능
        }
        break;
    case 3: //스톱워치 모드
        LCD_pos(0, 0);
        LCD_STR("Stop Watch      ");
        LCD_pos(1, 0);//시 분 초 순서로 출력
        LCD_char(s_hour1 + '0');
        LCD_char(s_hour2 + '0');
        LCD_char('h');
        LCD_char(s_min1 + '0');
        LCD_char(s_min2 + '0');
        LCD_char('m');
        LCD_char(s_sec1 + '0');
        LCD_char(s_sec2 + '0');
        LCD_char('.');
        LCD_char(point1 + '0');
        LCD_char(point2 + '0');
        LCD_char('s');
        break;
    case 4: //알람이 울렸을 때
        LCD_pos(0, 0);
        LCD_STR("WAKE UP!!!       ");
        LCD_pos(1, 0);
        LCD_STR("WAKE UP!!!       ");
        break;
    }
}


#pragma interrupt_handler timer0_ovf_isr:iv_TIM0_OVF
void timer0_ovf_isr(void) { //일반 시계 기능
    TCNT0 = 6;//분주비64로 설정-> 주기=0.000004초 ->250000회 카운트할 경우 1초
    cnt++; //250번 카운트 할 때마다 오버플로우 발생 -> 4번 발생하면 1초
    if (cnt >= 1000) {
        sec++; //초 증가
        cnt = 0;
    }
    if (sec == 60) {
        min++; //분 증가
        sec = 0;
    }
    if (min == 60) {
        hour++; //시 증가
        min = 0;
    }
    if (hour >= 24) {
        hour = 0;
        alarm = 0; //하루가 지나면 다시 알람이 울릴 수 있게 설정
        day++; //하루가 지나면 요일 증가
    }
}


#pragma interrupt_handler timer2_ovf_isr:iv_TIM2_OVF
void timer2_ovf_isr(void) {
    TCNT2 = 6; //분주비 64로 설정 -> 주기 = 0.000004초
    cnt2++;
    if (cnt2 >= 10) {
        point2++; //소수점 둘째자리 증가
        cnt2 = 0;
    }
    if (point2 == 10) {
        point1++; //소수점 첫째자리 증가
        point2 = 0;
    }
    if (point1 == 10) {
        sw_sec++; //스톱워치 초 증가
        point1 = 0;
    }
    if (sw_sec == 60) {
        sw_min++; //스톱워치 분 증가
        sw_sec = 0;
    }
    if (sw_min == 60) {
        sw_hour++; //스톱워치 시 증가
        sw_min = 0;
    }
    if (sw_hour == 24) {
        sw_hour = 0; //스톱워치가 24시를 넘어가면 0시로 다시 표기
    }
}


#pragma interrupt_handler int0_isr:iv_INT0
void int0_isr(void) {
    zulu++; //누를 경우 세계 표준시가 나옴(짝수인 경우), 한번 더 누를 경우 한국 시간이 나옴(홀수인 경우)
}


#pragma interrupt_handler int1_isr:iv_INT1
void int1_isr(void) {
    sw_cnt++; //한번 누를 경우 스톱워치 모드 (sw_cnt = 1)
    if (sw_cnt == 2) { //한번 더 누를 경우, 타이머2의 오버플로우 비트 enable(스톱워치 작동)
        TIMSK = 0x41;
    }
    else if (sw_cnt == 3) { //한번 더 누를 경우, 타이머2의 오버플로우 비트 disable(스톱워치 멈춤)
        TIMSK = 0x01;
    }
    else if (sw_cnt == 4) { //한번 더 누를경우 스톱워치 모드 종료
        sw_cnt = 0;
    }
}


#pragma interrupt_handler int2_isr:iv_INT2
void int2_isr(void) {
    if (sw_cnt != 0) { //스톱워치 모드일 경우에만 인터럽트 발생하게 설정
        sw_cnt = 1; //스톱워치 초기화를 할 경우, sw_cnt를 1로 설정해줌(스톱워치 모드)
        /*스톱워치 초기화*/
        sw_hour = 0;
        sw_min = 0;
        sw_sec = 0;
        point1 = 0;
        point2 = 0;
    }
}


#pragma interrupt_handler int3_isr:iv_INT3
void int3_isr(void) {
    hour++; //누를 경우 시를 1 증가
}


#pragma interrupt_handler int4_isr:iv_INT4
void int4_isr(void) {
    min++; //누를 경우 분을 1 증가
}


#pragma interrupt_handler int5_isr:iv_INT5
void int5_isr(void) {
    sec = 0; //누를 경우 초를 0으로 초기화
}


#pragma interrupt_handler int6_isr:iv_INT6
void int6_isr(void) {
    alarm = -1; //누를 경우 alarm = -1로 설정하여 하루가 지나기 전까지는 알람이 안 울리게 한다
    PORTA = 0x00; //포트A 0번핀에 LOW 출력 -> 알람이 꺼진다.
}