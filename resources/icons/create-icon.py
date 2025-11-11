#!/usr/bin/env python3
"""
Create Archivas Core icon with 3D effect
Creates multiple sizes for proper taskbar display
"""

from PIL import Image, ImageDraw, ImageFont
import os
import math

def create_icon_3d(size):
    """Create a 3D-style icon with metallic rim and neon green R"""
    # Create image with transparency
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    center = size // 2
    radius = int(size * 0.47)  # Slightly smaller to leave room for rim
    rim_width = max(2, int(size * 0.03))  # Rim width scales with size
    
    # Draw outer rim (metallic silver with gradient effect)
    # Create gradient effect for 3D rim
    for i in range(rim_width):
        alpha = 255 - (i * 20)
        gray = 200 - (i * 10)
        draw.ellipse([center - radius - rim_width + i, center - radius - rim_width + i,
                     center + radius + rim_width - i, center + radius + rim_width - i],
                    outline=(gray, gray, gray, alpha), width=1)
    
    # Draw dark purple circle (main background)
    draw.ellipse([center - radius, center - radius,
                 center + radius, center + radius],
                fill=(45, 27, 78, 255))  # Dark purple #2d1b4e
    
    # Draw inner shadow for 3D effect
    shadow_offset = max(1, size // 128)
    draw.ellipse([center - radius + shadow_offset, center - radius + shadow_offset,
                 center + radius - shadow_offset, center + radius - shadow_offset],
                outline=(30, 18, 60, 180), width=max(1, size // 64))
    
    # Draw neon green "R" with glow effect
    # Font size scales with icon size
    font_size = int(size * 0.62)
    
    try:
        # Try to use a bold font
        font_paths = [
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
        ]
        font = None
        for font_path in font_paths:
            if os.path.exists(font_path):
                try:
                    font = ImageFont.truetype(font_path, font_size)
                    break
                except:
                    continue
        if font is None:
            font = ImageFont.load_default()
    except:
        font = ImageFont.load_default()
    
    text = "R"
    
    # Get text bounding box
    bbox = draw.textbbox((0, 0), text, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    
    # Center the text
    x = center - text_width // 2
    y = center - text_height // 2 - int(size * 0.04)  # Slight upward adjustment
    
    # Draw glow effect (multiple layers for neon effect)
    glow_color = (0, 255, 136, 255)  # Neon green #00ff88
    for i in range(3, 0, -1):
        alpha = 100 - (i * 30)
        glow_size = font_size + (i * 2)
        try:
            glow_font = ImageFont.truetype(font_path, glow_size) if font != ImageFont.load_default() else font
        except:
            glow_font = font
        draw.text((x, y), text, fill=(0, 255, 136, alpha), font=glow_font, anchor="mm")
    
    # Draw main "R" text
    draw.text((x, y), text, fill=glow_color, font=font, anchor="mm")
    
    return img

def main():
    """Create icons in multiple sizes"""
    sizes = [16, 22, 24, 32, 48, 64, 96, 128, 256, 512]
    
    output_dir = os.path.dirname(os.path.abspath(__file__))
    
    print("Creating Archivas Core icons...")
    for size in sizes:
        icon = create_icon_3d(size)
        output_path = os.path.join(output_dir, f"archivas-core-{size}x{size}.png")
        icon.save(output_path, "PNG")
        print(f"  Created {size}x{size} icon")
    
    # Also create the main icon.png (512x512)
    icon = create_icon_3d(512)
    output_path = os.path.join(output_dir, "archivas-core.png")
    icon.save(output_path, "PNG")
    print(f"  Created archivas-core.png (512x512)")
    
    # Create icon.svg equivalent as high-res PNG
    icon = create_icon_3d(1024)
    output_path = os.path.join(output_dir, "archivas-core.svg.png")
    icon.save(output_path, "PNG")
    print(f"  Created high-res icon (1024x1024)")
    
    print("\nIcons created successfully!")
    print(f"Location: {output_dir}")

if __name__ == "__main__":
    main()

