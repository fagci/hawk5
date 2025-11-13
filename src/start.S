# Copyright 2023 Dual Tachyon
# https://github.com/DualTachyon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

	.syntax unified
	.cpu cortex-m0
	.fpu softvfp
	.thumb

	@ External symbols
	.global HandlerReset
	.global SystickHandler
	.weak SystickHandler

	@ Symbols from linker script
	.extern _estack
	.extern DATA_Init
	.extern BSS_Init
	.extern Main
    .extern __libc_init_array

@ ==============================================================================
@ Vector Table
@ ==============================================================================
	.section .isr_vector, "a", %progbits
	.type g_pfnVectors, %object
	.size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
	.word	_estack                    @ Initial Stack Pointer
	.word	HandlerReset               @ Reset Handler
	.word	HandlerNMI                 @ NMI Handler
	.word	HandlerHardFault           @ Hard Fault Handler
	.word	0                          @ Reserved
	.word	0                          @ Reserved
	.word	0                          @ Reserved
	.word	0                          @ Reserved
	.word	0                          @ Reserved
	.word	0                          @ Reserved
	.word	0                          @ Reserved
	.word	HandlerSVCall              @ SVCall Handler
	.word	0                          @ Reserved
	.word	0                          @ Reserved
	.word	HandlerPendSV              @ PendSV Handler
	.word	SystickHandler             @ SysTick Handler
	
	@ External Interrupts
	.word	HandlerWWDT                @ WWDT
	.word	HandlerIWDT                @ IWDT
	.word	HandlerRTC                 @ RTC
	.word	HandlerDMA                 @ DMA
	.word	HandlerSARADC              @ SARADC
	.word	HandlerTIMER_BASE0         @ TIMER_BASE0
	.word	HandlerTIMER_BASE1         @ TIMER_BASE1
	.word	HandlerTIMER_PLUS0         @ TIMER_PLUS0
	.word	HandlerTIMER_PLUS1         @ TIMER_PLUS1
	.word	HandlerPWM_BASE0           @ PWM_BASE0
	.word	HandlerPWM_BASE1           @ PWM_BASE1
	.word	HandlerPWM_PLUS0           @ PWM_PLUS0
	.word	HandlerPWM_PLUS1           @ PWM_PLUS1
	.word	HandlerUART0               @ UART0
	.word	HandlerUART1               @ UART1
	.word	HandlerUART2               @ UART2
	.word	HandlerSPI0                @ SPI0
	.word	HandlerSPI1                @ SPI1
	.word	HandlerIIC0                @ IIC0
	.word	HandlerIIC1                @ IIC1
	.word	HandlerCMP                 @ CMP
	.word	HandlerTIMER_BASE2         @ TIMER_BASE2
	.word	HandlerGPIOA5              @ GPIOA5
	.word	HandlerGPIOA6              @ GPIOA6
	.word	HandlerGPIOA7              @ GPIOA7
	.word	HandlerGPIOB0              @ GPIOB0
	.word	HandlerGPIOB1              @ GPIOB1
	.word	HandlerGPIOC0              @ GPIOC0
	.word	HandlerGPIOC1              @ GPIOC1
	.word	HandlerGPIOA               @ GPIOA
	.word	HandlerGPIOB               @ GPIOB
	.word	HandlerGPIOC               @ GPIOC

@ ==============================================================================
@ Reset Handler
@ ==============================================================================
	.section .text.HandlerReset
	.type HandlerReset, %function
HandlerReset:
	@ Initialize stack pointer from linker script
	ldr	r0, =_estack
	mov	sp, r0

    cpsid	i

	@ Copy .data section from FLASH to RAM
	bl	DATA_Init

	@ Zero .bss section
	bl	BSS_Init
    bl __libc_init_array

    cpsie	i

	@ Call main application
	bl	Main

	@ Infinite loop if Main returns
_Reset_Loop:
    wfi
	b	_Reset_Loop
	.size HandlerReset, .-HandlerReset

@ ==============================================================================
@ Default Interrupt Handlers
@ ==============================================================================
	.section .text.Default_Handler, "ax", %progbits
Default_Handler:
	b	Default_Handler
	.size Default_Handler, .-Default_Handler

@ ==============================================================================
@ Weak aliases for interrupt handlers
@ All handlers default to Default_Handler unless overridden
@ ==============================================================================
	.weak	HandlerNMI
	.thumb_set HandlerNMI, Default_Handler

	.weak	HandlerHardFault
	.thumb_set HandlerHardFault, Default_Handler

	.weak	HandlerSVCall
	.thumb_set HandlerSVCall, Default_Handler

	.weak	HandlerPendSV
	.thumb_set HandlerPendSV, Default_Handler

	.weak	SystickHandler
	.thumb_set SystickHandler, Default_Handler

	.weak	HandlerWWDT
	.thumb_set HandlerWWDT, Default_Handler

	.weak	HandlerIWDT
	.thumb_set HandlerIWDT, Default_Handler

	.weak	HandlerRTC
	.thumb_set HandlerRTC, Default_Handler

	.weak	HandlerDMA
	.thumb_set HandlerDMA, Default_Handler

	.weak	HandlerSARADC
	.thumb_set HandlerSARADC, Default_Handler

	.weak	HandlerTIMER_BASE0
	.thumb_set HandlerTIMER_BASE0, Default_Handler

	.weak	HandlerTIMER_BASE1
	.thumb_set HandlerTIMER_BASE1, Default_Handler

	.weak	HandlerTIMER_PLUS0
	.thumb_set HandlerTIMER_PLUS0, Default_Handler

	.weak	HandlerTIMER_PLUS1
	.thumb_set HandlerTIMER_PLUS1, Default_Handler

	.weak	HandlerPWM_BASE0
	.thumb_set HandlerPWM_BASE0, Default_Handler

	.weak	HandlerPWM_BASE1
	.thumb_set HandlerPWM_BASE1, Default_Handler

	.weak	HandlerPWM_PLUS0
	.thumb_set HandlerPWM_PLUS0, Default_Handler

	.weak	HandlerPWM_PLUS1
	.thumb_set HandlerPWM_PLUS1, Default_Handler

	.weak	HandlerUART0
	.thumb_set HandlerUART0, Default_Handler

	.weak	HandlerUART1
	.thumb_set HandlerUART1, Default_Handler

	.weak	HandlerUART2
	.thumb_set HandlerUART2, Default_Handler

	.weak	HandlerSPI0
	.thumb_set HandlerSPI0, Default_Handler

	.weak	HandlerSPI1
	.thumb_set HandlerSPI1, Default_Handler

	.weak	HandlerIIC0
	.thumb_set HandlerIIC0, Default_Handler

	.weak	HandlerIIC1
	.thumb_set HandlerIIC1, Default_Handler

	.weak	HandlerCMP
	.thumb_set HandlerCMP, Default_Handler

	.weak	HandlerTIMER_BASE2
	.thumb_set HandlerTIMER_BASE2, Default_Handler

	.weak	HandlerGPIOA5
	.thumb_set HandlerGPIOA5, Default_Handler

	.weak	HandlerGPIOA6
	.thumb_set HandlerGPIOA6, Default_Handler

	.weak	HandlerGPIOA7
	.thumb_set HandlerGPIOA7, Default_Handler

	.weak	HandlerGPIOB0
	.thumb_set HandlerGPIOB0, Default_Handler

	.weak	HandlerGPIOB1
	.thumb_set HandlerGPIOB1, Default_Handler

	.weak	HandlerGPIOC0
	.thumb_set HandlerGPIOC0, Default_Handler

	.weak	HandlerGPIOC1
	.thumb_set HandlerGPIOC1, Default_Handler

	.weak	HandlerGPIOA
	.thumb_set HandlerGPIOA, Default_Handler

	.weak	HandlerGPIOB
	.thumb_set HandlerGPIOB, Default_Handler

	.weak	HandlerGPIOC
	.thumb_set HandlerGPIOC, Default_Handler
