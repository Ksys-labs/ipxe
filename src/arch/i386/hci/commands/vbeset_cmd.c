#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/io.h>
#include <ipxe/umalloc.h>
#include <ipxe/malloc.h>
#include <ipxe/image.h>
#include <realmode.h>


FILE_LICENCE ( GPL2_OR_LATER );

/** "reboot" options */
struct vbeset_options {};

/** "reboot" option list */
static struct option_descriptor vbeset_opts[] = {};

static struct command_descriptor vbeset_cmd =
	COMMAND_DESC ( struct vbeset_options, vbeset_opts, 1, 1, "<mode>" );
	
struct vbe_controller
{
  unsigned char signature[4];
  unsigned short version;
  unsigned long oem_string;
  unsigned long capabilities;
  unsigned long video_mode;
  unsigned short total_memory;
  unsigned short oem_software_rev;
  unsigned long oem_vendor_name;
  unsigned long oem_product_name;
  unsigned long oem_product_rev;
  unsigned char reserved[222];
  unsigned char oem_data[256];
} __attribute__ ((packed));

struct vbe_mode
{
  unsigned short mode_attributes;
  unsigned char win_a_attributes;
  unsigned char win_b_attributes;
  unsigned short win_granularity;
  unsigned short win_size;
  unsigned short win_a_segment;
  unsigned short win_b_segment;
  unsigned long win_func;
  unsigned short bytes_per_scanline;

  /* >=1.2 */
  unsigned short x_resolution;
  unsigned short y_resolution;
  unsigned char x_char_size;
  unsigned char y_char_size;
  unsigned char number_of_planes;
  unsigned char bits_per_pixel;
  unsigned char number_of_banks;
  unsigned char memory_model;
  unsigned char bank_size;
  unsigned char number_of_image_pages;
  unsigned char reserved0;

  /* direct color */
  unsigned char red_mask_size;
  unsigned char red_field_position;
  unsigned char green_mask_size;
  unsigned char green_field_position;
  unsigned char blue_mask_size;
  unsigned char blue_field_position;
  unsigned char reserved_mask_size;
  unsigned char reserved_field_position;
  unsigned char direct_color_mode_info;

  /* >=2.0 */
  unsigned long phys_base;
  unsigned long reserved1;
  unsigned short reversed2;

  /* >=3.0 */
  unsigned short linear_bytes_per_scanline;
  unsigned char banked_number_of_image_pages;
  unsigned char linear_number_of_image_pages;
  unsigned char linear_red_mask_size;
  unsigned char linear_red_field_position;
  unsigned char linear_green_mask_size;
  unsigned char linear_green_field_position;
  unsigned char linear_blue_mask_size;
  unsigned char linear_blue_field_position;
  unsigned char linear_reserved_mask_size;
  unsigned char linear_reserved_field_position;
  unsigned long max_pixel_clock;

  unsigned char reserved3[189];
} __attribute__ ((packed));

static int get_vbe_controller_info(struct vbe_controller *controller)
{
	int       status;
	userptr_t buffer = real_to_user ( 0x0, 0x7c00 );
	
	memset(controller, 0, sizeof(struct vbe_controller));
	
	DBG( "get_vbe_controller_info: INT10/AX=0x4F00\n" );
	
	__asm__ __volatile__ ( REAL_CODE (
						"sti\n\t"
						"push %%di\n\t"
						"push %%es\n\t"
						"push $0x7c00\n\t"
						"pop %%di\n\t"
						"push $0\n\t"
						"pop %%es\n\t"
						"int $0x10\n\t"
						"pop %%es\n\t"
						"pop %%di\n\t"
						"cli\n\t"
					)
					: 	"=a" (status)
					: 	"a" ( 0x4F00 )
					/*:	"ebx", "ecx", "esi", "edi", "ebp"*/
	);
	
	DBG( "get_vbe_controller_info: INT10/AX=0x4F00 status: %04x\n", status);
	
	struct vbe_controller *_controller = (struct vbe_controller *)user_to_virt(buffer, 0);
	memcpy(controller, _controller, sizeof(struct vbe_controller));
	
	return status;
}

static int get_vbe_mode_info(int mode_number, struct vbe_mode *mode)
{
	int       status;
	userptr_t buffer = real_to_user ( 0x0, 0x7c00 );
	
	memset(mode, 0, sizeof(struct vbe_mode));

	DBG( "get_vbe_mode_info: INT10/AX=0x4F01 mode_number=0x%x\n", mode_number );
	
	__asm__ __volatile__ ( REAL_CODE (
						"sti\n\t"
						"push %%di\n\t"
						"push %%es\n\t"
						"push $0x7c00\n\t"
						"pop %%di\n\t"
						"push $0\n\t"
						"pop %%es\n\t"
						"int $0x10\n\t"
						"pop %%es\n\t"
						"pop %%di\n\t"
						"cli\n\t"
					)
					: 	"=a" (status)
					: 	"a" ( 0x4F01 ), "c" (mode_number)
	);
	
	DBG( "get_vbe_mode_info: INT10/AX=0x4F01 mode_number=0x%x status: %04x\n", mode_number, status);
	
	struct vbe_mode *_mode = (struct vbe_mode *)user_to_virt(buffer, 0);
	memcpy(mode, _mode, sizeof(struct vbe_mode));
	
	return status;
}

static int set_vbe_mode(int mode_number)
{
	int       status;

	DBG( "set_vbe_mode: INT10/AX=0x4F02 mode_number=0x%x\n", mode_number );
	
	__asm__ __volatile__ ( REAL_CODE (
						"sti\n\t"
						"andl $0xF7FF, %%ebx\n\t"
						"int $0x10\n\t"
						"cli\n\t"
					)
					: 	"=a" (status)
					: 	"a" ( 0x4F02 ), "b" (mode_number)
	);
	
	DBG( "set_vbe_mode: INT10/AX=0x4F02  mode_number=0x%x status: %04x\n", mode_number, status);
	
	return status;
}

static void get_vbe_pmif(int *pmif_segoff, int *pmif_len)
{
	int status;
	int addr, length;

	DBG( "set_vbe_mode: INT10/AX=0x4F0A\n" );
	
	__asm__ __volatile__ ( REAL_CODE (
						"sti\n\t"
						"push %%di\n\t"
						"push %%es\n\t"
						"int $0x10\n\t"
						"movw %%es, %%bx\n\t"
						"shll $16, %%ebx\n\t"
						"movw %%di, %%bx\n\t"
						"andl $0xffff, %%ecx\n\t"
						"pop %%es\n\t"
						"pop %%di\n\t"
						"cli\n\t"
					)
					: 	"=a" (status), "=b" (addr), "=c" (length)
					: 	"a" ( 0x4F0A ), "b" (0)
	);
	
	DBG( "set_vbe_mode: INT10/AX=0x4F0A status: %04x\n", status);
	
	DBG( "addr=%08x length=%d\n", addr, length);
	
	*pmif_segoff = addr;
	*pmif_len = length;
}

unsigned char *vbe_module = 0;
int ctrl_info_len = 0;
int mode_info_len = 0;
int mode_num = 0;
int pmif = 0;
int pmif_l = 0;

static int vbeset_exec ( int argc, char **argv ) {
	struct vbeset_options opts;
	int                   rc;
	unsigned int          mode_number;
	int                   pmif_segoff, pmif_len;
	struct vbe_controller controller;
	struct vbe_mode       mode;
	

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &vbeset_cmd, &opts ) ) != 0 )
		return rc;
	
	if ( ( rc = parse_integer ( argv[optind], &mode_number ) ) != 0 )
		return rc;
	
	DBG("VBE mode: 0x%x\n", mode_number);
	
	memcpy(controller.signature, "VBE2", 4);
	
	if (get_vbe_controller_info (&controller) != 0x004F)
	{
		printf (" VBE BIOS is not present.\n");
		return 1;
	}
	
	if (controller.version < 0x0200)
    {
		printf (" VBE version %d.%d is not supported.\n",
		   (int) (controller.version >> 8),
		   (int) (controller.version & 0xFF));
		return 1;
	}
    
    DBG (" VBE version %d.%d\n",
		   (int) (controller.version >> 8),
		   (int) (controller.version & 0xFF));
	
	if (get_vbe_mode_info (mode_number, &mode) != 0x004F
	  || (mode.mode_attributes & 0x0091) != 0x0091)
	{
		printf (" Mode 0x%x is not supported.\n", mode_number);
		return 1;
	}
	DBG("mode is supported\n");

	/* Now trip to the graphics mode.  */
	if (set_vbe_mode (mode_number | (1 << 14)) != 0x004F)
	{
		printf (" Switching to Mode 0x%x failed.\n", mode_number);
		return 1;
	}
	
	DBG("set mode id ok\n");
	
	get_vbe_pmif(&pmif_segoff, &pmif_len);
	ctrl_info_len = sizeof(struct vbe_controller);
	mode_info_len = sizeof(struct vbe_mode);

	vbe_module = malloc_dma(ctrl_info_len + mode_info_len, 4*1024);
	

	
	memcpy ( vbe_module, &controller, ctrl_info_len );
	memcpy ( vbe_module + ctrl_info_len, &mode, mode_info_len );
	
	mode_num = mode_number;
	pmif = pmif_segoff;
	pmif_l = pmif_len;
	
	printf ("   [VESA %d.%d info @ 0x%p(0x%lx), 0x%x bytes]\n",
	  controller.version >> 8, controller.version & 0xFF,
	  vbe_module, virt_to_bus(vbe_module), ctrl_info_len + mode_info_len);
	
	return 0;
}
	
struct command vbeset_command __command = {
	.name = "vbeset",
	.exec = vbeset_exec,
};