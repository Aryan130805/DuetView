/* ==========================================================================
   DuetView Official Website - Main JavaScript
   ========================================================================== */

document.addEventListener("DOMContentLoaded", () => {
  initMobileNavigation();
  initFaqAccordion();
  initDownloadTabs();
  initChecksumCopy();
  initScrollEffects();
});

/**
 * 1. Mobile Navigation Menu Toggle
 */
function initMobileNavigation() {
  const hamburger = document.querySelector(".hamburger");
  const navMenu = document.querySelector(".nav-menu");
  const navLinks = document.querySelectorAll(".nav-link, .nav-cta-btn");

  if (!hamburger || !navMenu) return;

  hamburger.addEventListener("click", () => {
    const isOpen = navMenu.classList.toggle("open");
    hamburger.setAttribute("aria-expanded", isOpen);
    document.body.style.overflow = isOpen ? "hidden" : "";
  });

  navLinks.forEach((link) => {
    link.addEventListener("click", () => {
      if (navMenu.classList.contains("open")) {
        navMenu.classList.remove("open");
        hamburger.setAttribute("aria-expanded", "false");
        document.body.style.overflow = "";
      }
    });
  });
}

/**
 * 2. FAQ Accordion Toggle
 */
function initFaqAccordion() {
  const faqItems = document.querySelectorAll(".faq-item");

  faqItems.forEach((item) => {
    const button = item.querySelector(".faq-question");
    if (!button) return;

    button.addEventListener("click", () => {
      const isOpen = item.classList.contains("open");

      // Close all other FAQ items for a clean accordion experience
      faqItems.forEach((other) => {
        if (other !== item) {
          other.classList.remove("open");
          const otherBtn = other.querySelector(".faq-question");
          if (otherBtn) otherBtn.setAttribute("aria-expanded", "false");
        }
      });

      // Toggle current item
      if (isOpen) {
        item.classList.remove("open");
        button.setAttribute("aria-expanded", "false");
      } else {
        item.classList.add("open");
        button.setAttribute("aria-expanded", "true");
      }
    });
  });
}

/**
 * 3. Download Section Tabs Switcher
 */
function initDownloadTabs() {
  const tabButtons = document.querySelectorAll(".tab-btn");
  const tabPanes = document.querySelectorAll(".download-tab-pane");

  if (!tabButtons.length || !tabPanes.length) return;

  tabButtons.forEach((btn) => {
    btn.addEventListener("click", () => {
      const targetTab = btn.getAttribute("data-tab");

      tabButtons.forEach((b) => b.classList.remove("active"));
      tabPanes.forEach((p) => p.classList.remove("active"));

      btn.classList.add("active");
      const activePane = document.getElementById(targetTab);
      if (activePane) {
        activePane.classList.add("active");
      }
    });
  });
}

/**
 * 4. Checksum Copy to Clipboard
 */
function initChecksumCopy() {
  const copyButtons = document.querySelectorAll(".copy-checksum-btn");

  copyButtons.forEach((btn) => {
    btn.addEventListener("click", () => {
      const textToCopy = btn.getAttribute("data-checksum");
      if (!textToCopy) return;

      navigator.clipboard.writeText(textToCopy).then(() => {
        const originalText = btn.innerHTML;
        btn.innerHTML = `<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="20 6 9 17 4 12"></polyline></svg> Copied!`;
        btn.classList.add("copied");

        setTimeout(() => {
          btn.innerHTML = originalText;
          btn.classList.remove("copied");
        }, 2000);
      }).catch((err) => {
        console.error("Clipboard copy failed:", err);
      });
    });
  });
}

/**
 * 5. Scroll Effects (Navbar Shadow, Active Links, Back-To-Top Button)
 */
function initScrollEffects() {
  const navbar = document.querySelector(".navbar");
  const backToTop = document.querySelector(".back-to-top");
  const sections = document.querySelectorAll("section[id]");
  const navLinks = document.querySelectorAll(".nav-link[href^='#']");

  window.addEventListener("scroll", () => {
    const scrollY = window.scrollY;

    // Navbar shadow elevation
    if (navbar) {
      if (scrollY > 20) {
        navbar.style.borderBottomColor = "#cbd5e1";
        navbar.style.boxShadow = "0 4px 20px rgba(0, 0, 0, 0.06)";
      } else {
        navbar.style.borderBottomColor = "var(--border-light)";
        navbar.style.boxShadow = "none";
      }
    }

    // Back to top button visibility
    if (backToTop) {
      if (scrollY > 450) {
        backToTop.classList.add("visible");
      } else {
        backToTop.classList.remove("visible");
      }
    }

    // Active nav link highlight
    sections.forEach((current) => {
      const sectionHeight = current.offsetHeight;
      const sectionTop = current.offsetTop - 120;
      const sectionId = current.getAttribute("id");

      if (scrollY > sectionTop && scrollY <= sectionTop + sectionHeight) {
        navLinks.forEach((link) => {
          if (link.getAttribute("href") === `#${sectionId}`) {
            link.classList.add("active");
          } else {
            link.classList.remove("active");
          }
        });
      }
    });
  });

  if (backToTop) {
    backToTop.addEventListener("click", () => {
      window.scrollTo({ top: 0, behavior: "smooth" });
    });
  }
}
